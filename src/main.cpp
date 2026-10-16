#include <sdk.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <Server/Components/Pawn/Impl/pawn_impl.hpp>
#include "natives.hpp"
#include "version.hpp"
#include <cstdio>
class IndexWeaverComponent final : public IComponent, public PawnEventHandler {
    IPawnComponent* pawn_ = nullptr;
    ICore* core_ = nullptr;
    index_weaver::Manager manager_;
public:
    PROVIDE_UID(0x66A7B41CA0D8F3CF);
    ~IndexWeaverComponent() override { cleanup(); }
    StringView componentName() const override { return "IndexWeaver"; }
    SemanticVersion componentVersion() const override { return {IW_VERSION_MAJOR,IW_VERSION_MINOR,IW_VERSION_PATCH,0}; }
    void onLoad(ICore* core) override { core_=core; index_weaver::bindNatives(&manager_); }
    void onInit(IComponentList* components) override {
        if(pawn_) return;
        pawn_=components->queryComponent<IPawnComponent>();
        if(!pawn_) { core_->printLn("[IndexWeaver] Pawn component unavailable"); return; }
        setAmxFunctions(pawn_->getAmxFunctions());
        pawn_->getEventDispatcher().addEventHandler(this);
    }
    void onReady() override {}
    void onFree(IComponent* component) override {
        if(component==pawn_) {
            pawn_->getEventDispatcher().removeEventHandler(this); pawn_=nullptr;
            manager_.shutdown(); setAmxFunctions();
        }
    }
    void reset() override { manager_.reset(); }
    void free() override { delete this; }
    void onAmxLoad(IPawnScript& script) override {
        auto* amx=script.GetAMX();
        try {
            manager_.load(amx);
            // AMX_ERR_NOTFOUND can mean other components still have natives to register.
            const auto result=index_weaver::registerNatives(amx);
            if(result!=AMX_ERR_NONE && result!=AMX_ERR_NOTFOUND) {
                core_->printLn("[IndexWeaver] Native registration failed (%d)",result); manager_.unload(amx);
            }
        } catch(const std::exception& e) {
            manager_.unload(amx); core_->printLn("[IndexWeaver] Script initialization failed: %s",e.what());
        } catch(...) {
            manager_.unload(amx); core_->printLn("[IndexWeaver] Unexpected script initialization failure");
        }
    }
    void onAmxUnload(IPawnScript& script) override { manager_.unload(script.GetAMX()); }
private:
    void cleanup() noexcept {
        if(pawn_) { pawn_->getEventDispatcher().removeEventHandler(this); pawn_=nullptr; }
        index_weaver::bindNatives(nullptr); manager_.shutdown(); setAmxFunctions();
    }
};
COMPONENT_ENTRY_POINT() { return new IndexWeaverComponent(); }
