#include "config-generic.h"

#include <variant>

// Machine generated by https://github.com/davidchisnall/config-gen DO NOT EDIT
#ifdef CONFIG_NAMESPACE_BEGIN
CONFIG_NAMESPACE_BEGIN
#endif
class AzureInstance
{
	UCLPtr obj;

	public:
	AzureInstance(const ucl_object_t *o) : obj(o) {}
	class computeClass
	{
		UCLPtr obj;

		public:
		computeClass(const ucl_object_t *o) : obj(o) {}
		class osProfileClass
		{
			UCLPtr obj;

			public:
			osProfileClass(const ucl_object_t *o) : obj(o) {}
			std::string_view adminUsername() const CONFIG_LIFETIME_BOUND
			{
				return StringViewAdaptor(obj["adminUsername"]);
			}

			std::string_view computerName() const CONFIG_LIFETIME_BOUND
			{
				return StringViewAdaptor(obj["computerName"]);
			}

			std::string_view
			disablePasswordAuthentication() const CONFIG_LIFETIME_BOUND
			{
				return StringViewAdaptor(obj["disablePasswordAuthentication"]);
			}
		};
		class publicKeysItemClass
		{
			UCLPtr obj;

			public:
			publicKeysItemClass(const ucl_object_t *o) : obj(o) {}
			std::string_view keyData() const CONFIG_LIFETIME_BOUND
			{
				return StringViewAdaptor(obj["keyData"]);
			}

			std::string_view path() const CONFIG_LIFETIME_BOUND
			{
				return StringViewAdaptor(obj["path"]);
			}
		};
		class storageProfileClass
		{
			UCLPtr obj;

			public:
			storageProfileClass(const ucl_object_t *o) : obj(o) {}
			class resourceDiskClass
			{
				UCLPtr obj;

				public:
				resourceDiskClass(const ucl_object_t *o) : obj(o) {}
				std::string_view size() const CONFIG_LIFETIME_BOUND
				{
					return StringViewAdaptor(obj["size"]);
				}
			};
			resourceDiskClass resourceDisk() const
			{
				return resourceDiskClass(obj["resourceDisk"]);
			}
		};
		std::string_view name() const CONFIG_LIFETIME_BOUND
		{
			return StringViewAdaptor(obj["name"]);
		}

		osProfileClass osProfile() const
		{
			return osProfileClass(obj["osProfile"]);
		}

		::Range<publicKeysItemClass, publicKeysItemClass, true>
		publicKeys() const
		{
			return ::Range<publicKeysItemClass, publicKeysItemClass, true>(
			  obj["publicKeys"]);
		}

		storageProfileClass storageProfile() const
		{
			return storageProfileClass(obj["storageProfile"]);
		}
	};
	computeClass compute() const
	{
		return computeClass(obj["compute"]);
	}
};
#ifdef CONFIG_NAMESPACE_END
CONFIG_NAMESPACE_END
#endif