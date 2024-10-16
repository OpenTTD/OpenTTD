/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file provider_manager.h Definition of the ProviderManager */

#ifndef PROVIDER_MANAGER_H
#define PROVIDER_MANAGER_H

/**
 * The ProviderManager manages a single Provider-type.
 *
 * It allows for automatic registration and unregistration of providers.
 *
 * @tparam T Service provider type.
 */
template <typename TProviderType>
class ProviderManager {
public:
	/* Avoid copying this object; it is a singleton object. */
	ProviderManager(ProviderManager const &) = delete;

	ProviderManager &operator=(ProviderManager const &) = delete;

	static void Register(TProviderType &instance)
	{
		/* Insert according to comparator. */
		auto &providers = GetProviders();
		auto it = std::ranges::lower_bound(providers, &instance, typename TProviderType::ProviderSorter());
		providers.insert(it, &instance);
	}

	static void Unregister(TProviderType &instance)
	{
		auto &providers = GetProviders();
		providers.erase(std::find(std::begin(providers), std::end(providers), &instance));
	}

	/**
	* Get the currently known sound loaders.
	* @return The known sound loaders.
	*/
	static std::vector<TProviderType *> &GetProviders()
	{
		static std::vector<TProviderType *> providers{};
		return providers;
	}
};

template <typename T>
class BaseProvider {
public:
	constexpr BaseProvider(std::string_view name, std::string_view description) : name(name), description(description) {}
	virtual ~BaseProvider() {}

	inline std::string_view GetName() const { return this->name; }
	inline std::string_view GetDescription() const { return this->description; }

	/**
	 * Sorter for BaseProvider.
	 *
	 * It will sort based on the name. If the name is the
	 * same, it will sort based on the pointer value.
	 */
	struct ProviderSorter {
		bool operator()(const T *a, const T *b) const
		{
			if (a->GetName() == b->GetName()) return a < b;
			return a->GetName() < b->GetName();
		}
	};

protected:
	const std::string_view name;
	const std::string_view description;
};

template <typename T>
class PriorityBaseProvider : public BaseProvider<T> {
public:
	constexpr PriorityBaseProvider(std::string_view name, std::string_view description, int priority) : BaseProvider<T>(name, description), priority(priority) {}
	virtual ~PriorityBaseProvider() {}

	inline int GetPriority() const { return this->priority; }

	/**
	 * Sorter for PriorityBaseProvider.
	 *
	 * It will sort based on the priority, smaller first. If the priority is the
	 * same, it will sort based on the pointer value.
	 */
	struct ProviderSorter {
		bool operator()(const T *a, const T *b) const
		{
			if (a->GetPriority() == b->GetPriority()) return a < b;
			return a->GetPriority() < b->GetPriority();
		}
	};

protected:
	const int priority;
};

#endif /* PROVIDER_MANAGER_H */
