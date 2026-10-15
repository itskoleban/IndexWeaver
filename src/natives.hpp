#pragma once
#include "manager.hpp"
#include <amx/amx.h>
namespace index_weaver {
void bindNatives(Manager* manager) noexcept;
int registerNatives(AMX* amx);
}
