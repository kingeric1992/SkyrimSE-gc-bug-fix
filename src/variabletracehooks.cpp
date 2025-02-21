#include <unordered_set>

#include "variabletracehooks.h"
#include "trace.h"

using namespace Trace;

namespace VariableTraceHooks
{
	REL::Relocation<__int64 __fastcall(RE::BSScript::Object*)> orig_BSScript__Object__DecRef;
	__int64 __fastcall hk_BSScript__Object__DecRef(RE::BSScript::Object* thisptr)
	{
		char buffer[1024]{};
		if (ShouldPrintGCInfo())
		{
			RE::BSFixedString handleStr("unk");

			// persistant
			static RE::BSScript::IObjectHandlePolicy* pPolicy{};
			if (auto pvm = RE::BSScript::Internal::VirtualMachine::GetSingleton(); 
				pPolicy || (pvm && (pPolicy = pvm->GetObjectHandlePolicy())))
				pPolicy->ConvertHandleToString(thisptr->GetHandle(), handleStr);

			if (auto tt = thisptr->GetTypeInfo(); tt && tt->GetName())
				sprintf_s(buffer, "[%s %s]", tt->GetName(), handleStr.data());
		}

		auto result = orig_BSScript__Object__DecRef(thisptr);

		if (ShouldPrintGCInfo()) {
			if (result == 0)
				Print("Released an object: 0x%p %s", thisptr, buffer);
		}

		return result;
	}

	REL::Relocation<__int64 __fastcall(RE::BSScript::Variable*)> orig_BSScript__Variable__Cleanup;
	void __fastcall hk_BSScript__Variable__Cleanup(RE::BSScript::Variable* thisptr)
	{
		if (ShouldPrintGCInfo())
			Print("Cleaning up a variable: 0x%p", thisptr);

		orig_BSScript__Variable__Cleanup(thisptr);
	}

#define MAKE_HOOK(name, relid, instrlen) orig_##name = WriteHookWithReturn(relid, instrlen, hk_##name);

	void InstallHooks()
	{
		MAKE_HOOK(BSScript__Object__DecRef, (REL::ID)REL::RelocationID(97469, 104253), 5);  //26ED210(1.10.163) 1234410(1.5.97) 1422F50
		//MAKE_HOOK(BSScript__Variable__Cleanup, (REL::ID)REL::RelocationID(97508, 104296), 5);  //14259F0(1.6.1170) 1236d10(1.5.97) 97508

		logger::info("Installed object trace hooks");
	}

#undef MAKE_HOOK
}
