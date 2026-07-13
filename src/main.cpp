/**
 * @file main.cpp
 * @brief Entry point and component lifecycle management for the IndexWeaver open.mp plugin.
 *
 * This file defines the main component class that integrates with the open.mp
 * server lifecycle and registers the Pawn event handlers and native functions.
 */

// Include the official open.mp C++ SDK headers
#include <sdk.hpp>

// Include Pawn AMX native registration headers
#include <pawn-natives/NativeFunc.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <Server/Components/Pawn/Impl/pawn_impl.hpp>

// Include our custom hybrid storage engine and native declarations
#include "mapper_store.hpp"
#include "natives.hpp"

// Global pointer to the registry store. Native functions (in natives.cpp) will use this pointer to
// access the storage engine.
index_weaver::IndexRegistryStore* gIndexWeaverStore = nullptr;

/**
 * @class IndexWeaverComponent
 * @brief Manages the open.mp component integration and Pawn AMX lifecycle.
 *
 * It is responsible for attaching natives to Pawn scripts when they load,
 * managing the globally accessible IndexRegistryStore, and cleaning up
 * allocations when the server resets or unloads the plugin.
 */
class IndexWeaverComponent final : public IComponent, public PawnEventHandler
{
  private:
	// Pointer to the open.mp Pawn component used to hook AMX events
	IPawnComponent* pawn_ = nullptr;

	// The actual instance of our hybrid storage engine
	index_weaver::IndexRegistryStore registry_;

	// Flag to track whether the component has been fully initialized
	bool initialized_ = false;

  public:
	// Provide a unique 64-bit identifier for this open.mp component (Generated randomly)
	PROVIDE_UID(0x66A7B41CA0D8F3CF);

	// Destructor: ensures resources are properly cleaned up upon termination
	~IndexWeaverComponent() override
	{
		cleanup();
	}

	// Returns the name of this component as it will appear in server logs
	StringView componentName() const override
	{
		return "IndexWeaver";
	}

	// Returns the semantic version of this component (Major, Minor, Patch, Build)
	SemanticVersion componentVersion() const override
	{
		return {1, 0, 0, 0};
	}

	// Called very early in the server startup process.
	// This is the safest place to assign the global store pointer.
	void onLoad(ICore* /*core*/) override
	{
		gIndexWeaverStore = &registry_;
		initialized_ = true;
	}

	// Called to resolve component dependencies.
	// We use this to retrieve the PawnComponent and hook our native functions.
	void onInit(IComponentList* components) override
	{
		// Attempt to grab the Pawn component instance from the server list
		pawn_ = components->queryComponent<IPawnComponent>();

		// If the server doesn't have the Pawn component loaded, we cannot proceed
		if (!pawn_)
		{
			return;
		}

		// Register our native functions into the global AMX function pool
		setAmxFunctions(pawn_->getAmxFunctions());

		// Add this class as an event handler to listen for AMX load/unload events
		pawn_->getEventDispatcher().addEventHandler(this);
	}

	// Called when all components are fully initialized and ready. Not needed for this plugin.
	void onReady() override {}

	// Called when another component is being freed from memory.
	void onFree(IComponent* component) override
	{
		// If the component being freed is the Pawn component, we must detach our hooks
		if (component == pawn_ && pawn_)
		{
			pawn_->getEventDispatcher().removeEventHandler(this);
			pawn_ = nullptr;

			// Clear the AMX native function registry
			setAmxFunctions();
		}
	}

	// Called when the server triggers a soft reset (e.g., 'gmx' command)
	void reset() override
	{
		if (initialized_)
		{
			// Wipe all in-memory registries to prevent dangling pointers in the new game mode
			registry_.clearAll();
		}
	}

	// Called when the server is explicitly destroying this component
	void free() override
	{
		cleanup();
		delete this; // Self-destruction as mandated by the IComponent contract
	}

	// Called every time a new Pawn script (GameMode or FilterScript) is loaded into memory
	void onAmxLoad(IPawnScript& script) override
	{
		if (initialized_)
		{
			// Inject our registered native functions into this specific AMX instance
			pawn_natives::AmxLoad(script.GetAMX());
		}
	}

	// Called when a Pawn script is unloaded (e.g., FilterScript unload). We don't need to do
	// anything.
	void onAmxUnload(IPawnScript&) override {}

  private:
	// Centralized cleanup method to release memory and detach hooks safely
	void cleanup() noexcept
	{
		// Prevent double-free or cleaning up uninitialized memory
		if (!initialized_)
		{
			return;
		}

		// Detach from the Pawn component event dispatcher if attached
		if (pawn_)
		{
			pawn_->getEventDispatcher().removeEventHandler(this);
			pawn_ = nullptr;
		}

		// Nullify the global pointer to prevent natives from accessing destroyed memory
		if (gIndexWeaverStore == &registry_)
		{
			gIndexWeaverStore = nullptr;
		}

		// Hard-clear all hybrid storage maps and arrays
		registry_.clearAll();

		// Mark component as uninitialized
		initialized_ = false;
	}
};

// Suppress GCC/Clang warnings about 'cdecl' attribute being ignored on non-Windows platforms
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

// Expose the creation function for the open.mp component loader to find and initialize
COMPONENT_ENTRY_POINT()
{
	return new IndexWeaverComponent();
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif