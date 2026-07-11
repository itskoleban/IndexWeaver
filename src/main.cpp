#include <sdk.hpp>
#include <pawn-natives/NativeFunc.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <Server/Components/Pawn/Impl/pawn_impl.hpp>

#include "mapper_store.hpp"
#include "natives.hpp"

index_weaver::IndexRegistryStore* gIndexWeaverStore = nullptr;

class IndexWeaverComponent final : public IComponent, public PawnEventHandler
{
private:
    IPawnComponent* pawn_ = nullptr;
    index_weaver::IndexRegistryStore registry_;
    bool initialized_ = false;

public:
    PROVIDE_UID(0x66A7B41CA0D8F3CF);

    ~IndexWeaverComponent() override
    {
        cleanup();
    }

    StringView componentName() const override { return "IndexWeaver"; }
    SemanticVersion componentVersion() const override { return {1, 0, 0, 0}; }

    void onLoad(ICore* core) override
    {
        gIndexWeaverStore = &registry_;
        initialized_ = true;
    }

    void onInit(IComponentList* components) override
    {
        pawn_ = components->queryComponent<IPawnComponent>();
        if (!pawn_) return;

        setAmxFunctions(pawn_->getAmxFunctions());
        pawn_->getEventDispatcher().addEventHandler(this);
    }

    void onReady() override {}

    void onFree(IComponent* component) override
    {
        if (component == pawn_)
        {
            pawn_->getEventDispatcher().removeEventHandler(this);
            pawn_ = nullptr;
            setAmxFunctions();
        }
    }

    void reset() override
    {
        if (initialized_) registry_.clearAll();
    }

    void free() override
    {
        cleanup();
        delete this;
    }

    void onAmxLoad(IPawnScript& script) override
    {
        if (initialized_) pawn_natives::AmxLoad(script.GetAMX());
    }

    void onAmxUnload(IPawnScript&) override {}

private:
    void cleanup() noexcept
    {
        if (!initialized_) return;

        if (pawn_)
        {
            pawn_->getEventDispatcher().removeEventHandler(this);
            pawn_ = nullptr;
        }

        if (gIndexWeaverStore == &registry_)
        {
            gIndexWeaverStore = nullptr;
        }
        
        registry_.clearAll();
        initialized_ = false;
    }
};

COMPONENT_ENTRY_POINT()
{
    return new IndexWeaverComponent();
}