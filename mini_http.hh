// Copyright David Chisnall
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <optional>
#include <regex>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <variant>

namespace
{
	/**
	 * Minimal HTTP client that can perform GET requests for API calls.
	 */
	class HTTPClient
	{
		/**
		 * Streambuf implementation that wraps a socket for synchronous
		 * communication.
		 *
		 * The template parameter is the size of the input and output buffers.
		 * The default is chosen by fair die roll.
		 */
		template<size_t BufferSize = 128>
		class sockbuf : public std::streambuf
		{
			/**
			 * The file descriptor of the socket.
			 */
			int sock = -1;

			/**
			 * The input buffer.
			 */
			std::array<char_type, BufferSize> in_buffer;

			/**
			 * The output buffer.
			 */
			std::array<char_type, BufferSize> out_buffer;

			/**
			 * Underflow.  Called when there is no data in the buffer.  This
			 * blocks until data is available or until timeout.
			 */
			int_type underflow() override
			{
				// Read from the socket.
				ssize_t size = read(sock, in_buffer.data(), in_buffer.size());
				// Uncomment for debugging
				// write(STDOUT_FILENO, in_buffer.data(), size);
				// 0 indicates EOF, report it immediately.
				if (size == 0)
				{
					return traits_type::eof();
				}
				// -1 indicates error
				if (size == -1)
				{
					// If we were interrupted, retry.
					if ((errno == EINTR) || (errno == EAGAIN))
					{
						return underflow();
					}
					// For every other kind of error, just give up.
					return traits_type::eof();
				}
				// Provide the data that we've read to the superclass
				setg(
				  in_buffer.data(), in_buffer.begin(), in_buffer.data() + size);
				// Return the first byte that we've read (part of the API
				// contract).
				return traits_type::to_int_type(in_buffer[0]);
			}

			int sync(bool allowPartial)
			{
				// Get the current insert point in the output buffer.
				char_type *current = pptr();
				// If the buffer hasn't been allocated yet then sync is a
				// no-op.
				if (current == nullptr)
				{
					// Success, trivially: writing 0 bytes always succeeds.
					return 0;
				}
				// Check some invariants.  If there is a buffer, it should
				// always be one that we expect.
				assert(current >= out_buffer.begin());
				assert(current < out_buffer.end());
				// Calculate the amount of data that we want to send.
				size_t bytes = current - out_buffer.begin();
				// If we have a valid buffer but still have no data to write,
				// don't bother with the system call.
				if (bytes == 0)
				{
					return 0;
				}
				// Write as much as we have.
				ssize_t size = write(sock, out_buffer.data(), bytes);

				// Uncomment for debugging
				// write(STDOUT_FILENO, out_buffer.data(), size);

				// If we wrote some
				if (size > 0)
				{
					// Calculate the amount that we didn't write
					size_t remainder = bytes - size;
					// Copy that chunk to the start of the buffer.
					std::copy(out_buffer.end() - size,
					          out_buffer.end(),
					          out_buffer.begin());
					// Mark the space after it as the available buffer.
					setp(out_buffer.begin() + remainder, out_buffer.end());
					// Try to write the remainder if we're not doing a full
					// sync.
					if (remainder && !allowPartial)
					{
						return sync();
					}
					// Zero return indicates success
					return 0;
				}
				// If we got a transient failure, try again.
				if ((size == 0) || (errno == EINTR) || (errno == EAGAIN))
				{
					return sync();
				}
				// If we got a permanent error, report it.
				return -1;
			}

			/**
			 * Synchronise, requires that all data in the output buffer has been
			 * flushed.
			 */
			int sync() override
			{
				return sync(false);
			}

			/**
			 * Overflow.  Called when there is no space left in the write
			 * buffer. The argument is the first character to store.
			 */
			int_type overflow(int_type ch = traits_type::eof()) override
			{
				// If this is the first call, allocate the buffer.
				if (pptr() == nullptr)
				{
					setp(out_buffer.begin(), out_buffer.end());
					sputc(ch);
					return 1;
				}
				// Try to write data.  We don't need to write the entire buffer
				// if there isn't space in the socket buffer, so accept partial
				// writes.
				if (sync(true) == 0)
				{
					sputc(ch);
					return 1;
				}
				// If sync failed, returning eof indicates error.
				return traits_type::eof();
			}

			/**
			 * The default set of hints to pass to `getaddrinfo`.  Request
			 * `SOCK_STREAM` but do not add any other requirements.
			 */
			static constexpr addrinfo default_hints()
			{
				addrinfo ai    = {0};
				ai.ai_socktype = SOCK_STREAM;
				return ai;
			}

			public:
			/**
			 * Constructor, takes an initialised socket.
			 */
			sockbuf(int s) : sock(s) {}

			/**
			 * Construct a client socket with the specified host and port.  The
			 * hints are passed to `getaddrinfo` and specify constraints on the
			 * connection. By default, this requires a stream socket but imposes
			 * no other constraints.
			 *
			 * If connection fails then a subsequent call to `is_valid` will
			 * return `false`.
			 */
			sockbuf(const char *host,
			        const char *service = "http",
			        addrinfo    hints   = default_hints())
			{
				addrinfo *info;
				if (getaddrinfo(host, service, &hints, &info) != 0)
				{
					return;
				}
				for (addrinfo *i = info; i != nullptr; i = i->ai_next)
				{
					sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
					if (sock < 0)
					{
						continue;
					}
					if (connect(sock, i->ai_addr, i->ai_addrlen) >= 0)
					{
						break;
					}
					close(sock);
					sock = -1;
				}
				freeaddrinfo(info);
			}

			/**
			 * Returns whether the socket is a valid file descriptor number.
			 * This may return true even if writing to the socket would fail.
			 */
			bool is_valid()
			{
				return sock >= 0;
			}

			/**
			 * Destructor.  Writes any pending output and closes the socket.
			 */
			~sockbuf()
			{
				sync(false);
				close(sock);
			}
		};

		/**
		 * The host that we're connected to.
		 */
		std::string host;

		/**
		 * The streambuf that wraps the socket that we're connecting to.
		 */
		sockbuf<1024> buf;

		/**
		 * The stream for connecting to the server.
		 */
		std::iostream connection;

		/**
		 * We use a lot of string views in our APIs, import the type into our
		 * private namespace.
		 */
		using string_view = std::string_view;

		/**
		 * Write a single header to the stream.
		 */
		void write_header(string_view name, string_view value)
		{
			connection << name << ": " << value << "\r\n";
		}

		public:
		/**
		 * Empty collection of headers.  This can be replaced with an empty
		 * range once the ranges TS is widely supported.
		 */
		static inline constexpr std::array<std::pair<string_view, string_view>,
		                                   0>
		  no_headers{};

		/**
		 * Callback that does nothing, used when one is not specified.
		 */
		static void no_header_callback(string_view name, string_view value) {}

		/**
		 * Class encapsulating an HTTP error value.
		 */
		struct HTTPError
		{
			/**
			 * The HTTP status code.
			 */
			int error_code;

			/**
			 * Any accompanying error message.
			 */
			std::string error_message;
		};

		/**
		 * Class wrapping an HTTP error or a result of type T.
		 */
		template<typename T>
		class ErrorOr
		{
			/**
			 * The contents of the result.
			 */
			std::variant<const HTTPError, T> result;

			public:
			/**
			 * Constructor from an error.
			 */
			ErrorOr(const HTTPError &&e) : result(std::move(e)) {}

			/**
			 * Constructor from a valid result.
			 */
			ErrorOr(T &&t) : result(std::move(t)) {}

			/**
			 * Call operator.  Takes one or two arguments.  The first argument
			 * is invoked with a reference to the result type if this contains a
			 * valid result.  The second argument, if provided, is invoked with
			 * the `HTTPError`, if this contains an error value.
			 *
			 * For example:
			 *
			 * ```c++
			 * auto result = con.get(...);
			 * result([&](auto &buffer) { ... });
			 * result([&](auto &buffer) { ... }, [&](auto &error) { ... });
			 * ```
			 */
			template<
			  typename THandler,
			  typename ErrorHandler = std::function<void(const HTTPError &)>>
			void operator()(
			  THandler &&    th,
			  ErrorHandler &&eh = [](const HTTPError &) {})
			{
				std::visit(
				  [&](auto &&arg) {
					  using ArgType = std::decay_t<decltype(arg)>;
					  if constexpr (std::is_same_v<ArgType, HTTPError>)
					  {
						  eh(std::forward<decltype(arg)>(arg));
					  }
					  else if constexpr (std::is_same_v<ArgType, T>)
					  {
						  th(std::forward<decltype(arg)>(arg));
					  }
				  },
				  result);
			}
		};

		/**
		 * Constructor.  Creates a socket and connects to the specified host, on
		 * the given port.
		 *
		 * Note: This class does not support any protocols other than HTTP/1.1.
		 * You cannot change the protocol to, for example, HTTPS, by changing
		 * the port.
		 */
		HTTPClient(std::string &&host, std::string port = "http")
		  : host(host), buf(host.c_str(), port.c_str()), connection(&buf)
		{
		}

		/**
		 * Perform a single HTTP GET request with the specified resource.
		 *
		 * Optionally provides a list of headers to send and a callback for
		 * handling headers that are received.
		 *
		 * The sent headers must be an iterable collection of pair-like objects
		 * containing something convertible to a `std::string_view`.
		 *
		 * The callback must take two string_view arguments.  If the headers
		 * need to be preserved beyond the lifetime of the callback then the
		 * callback is responsible for copying them.
		 */
		template<typename Headers  = decltype(no_headers),
		         typename Callback = decltype(no_header_callback)>
		ErrorOr<std::vector<char>>
		get(string_view    resource,
		    const Headers &headers         = no_headers,
		    Callback &     header_callback = no_header_callback)
		{
			connection << "GET " << resource << " HTTP/1.1\r\n";
			write_header("Host", host);
			for (auto &h : headers)
			{
				write_header(std::get<0>(h), std::get<1>(h));
			}
			connection << "\r\n" << std::flush;

			size_t      length = 0;
			std::string line;
			std::getline(connection, line);
			if (!line.starts_with("HTTP/1.1 ") || (line.size() < 12))
			{
				return HTTPError{505, "Invalid HTTP response"};
			}
			if (line.substr(9, 3) != "200")
			{
				HTTPError err;
				err.error_code = stol(line.substr(9, 3));
				if (line.size() >= 13)
				{
					err.error_message = line.substr(13);
				}
				return err;
			}
			while (std::getline(connection, line))
			{
				if (line == "\r")
				{
					break;
				}
				size_t colon = line.find(':');
				if (colon == std::string::npos)
				{
					continue;
				}
				std::string header_name = line.substr(0, colon);
				std::string header_data =
				  line.substr(colon + 1, line.size() - colon - 1);
				if (header_name == "Content-Length")
				{
					length = std::stoll(header_data);
				}
				header_callback(header_name, header_data);
			}

			std::vector<char> buffer;
			buffer.resize(length);
			connection.read(buffer.data(), buffer.size());
			return buffer;
		}
	};
}
