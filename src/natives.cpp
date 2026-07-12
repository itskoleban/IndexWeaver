#include "natives.hpp"
#include "mapper_store.hpp"

#include <Server/Components/Pawn/Impl/pawn_natives.hpp>

SCRIPT_API(SetMapIndex, bool(int registryType, int id, int index))
{
	return gIndexWeaverStore ? gIndexWeaverStore->set(registryType, id, index) : false;
}

SCRIPT_API(GetMapIndex, int(int registryType, int id))
{
	return gIndexWeaverStore ? gIndexWeaverStore->get(registryType, id)
							 : index_weaver::IndexRegistryStore::INVALID_INDEX;
}

SCRIPT_API(RemoveMapIndex, bool(int registryType, int id))
{
	return gIndexWeaverStore ? gIndexWeaverStore->remove(registryType, id) : false;
}

SCRIPT_API(HasMapIndex, bool(int registryType, int id))
{
	return gIndexWeaverStore ? gIndexWeaverStore->has(registryType, id) : false;
}

SCRIPT_API(ClearMapRegistry, bool(int registryType))
{
	return gIndexWeaverStore ? gIndexWeaverStore->clearType(registryType) : false;
}

SCRIPT_API(ClearAllMapRegistries, void())
{
	if (gIndexWeaverStore)
	{
		gIndexWeaverStore->clearAll();
	}
}

SCRIPT_API(ReserveMapRegistry, bool(int registryType, int capacity))
{
	return (gIndexWeaverStore && capacity > 0)
			   ? gIndexWeaverStore->reserve(registryType, static_cast<std::size_t>(capacity))
			   : false;
}

SCRIPT_API(SetMapDebugLevel, bool(int level))
{
	if (!gIndexWeaverStore)
	{
		return false;
	}

	if (level < 0 || level > 2)
	{
		return false;
	}

	gIndexWeaverStore->setDebugLevel(
		static_cast<index_weaver::IndexRegistryStore::DebugLevel>(level));

	return true;
}

SCRIPT_API(GetMapDebugLevel, int())
{
	if (!gIndexWeaverStore)
	{
		return 0;
	}

	return static_cast<int>(gIndexWeaverStore->debugLevel());
}