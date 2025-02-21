#include "gctracehooks.h"
#include "trace.h"

using namespace std::chrono;
using namespace Trace;

namespace GCTraceHooks
{
	//
	// SaveCleanup
	//
	REL::Relocation<void __fastcall(RE::BSScript::Internal::VirtualMachine*)> orig_BSScript__Internal__VirtualMachine__SaveCleanup;
	void __fastcall hk_BSScript__Internal__VirtualMachine__SaveCleanup(RE::BSScript::Internal::VirtualMachine* thisptr)
	{
		EnableFullGCInfo(true);

		if (ShouldPrintGCInfo()) {
			Print("===============================================");
			Print("Begin SaveCleanup");
			Print("===============================================");
			IncPadding();
		}

		auto timerStart = high_resolution_clock::now();
		orig_BSScript__Internal__VirtualMachine__SaveCleanup(thisptr);
		auto timerEnd = high_resolution_clock::now();

		if (ShouldPrintGCInfo()) {
			DecPadding();
			Print("===============================================");
			Print("End SaveCleanup (%lld ms)", duration_cast<milliseconds>(timerEnd - timerStart).count());
			Print("===============================================");
		}

		EnableFullGCInfo(false);
	}

	//
	// LoadCleanup
	//
	REL::Relocation<void __fastcall(RE::BSScript::IVMSaveLoadInterface*)> orig_BSScript__IVMSaveLoadInterface__LoadCleanup;
	void __fastcall hk_BSScript__IVMSaveLoadInterface__LoadCleanup(RE::BSScript::IVMSaveLoadInterface* thisptr)
	{
		EnableFullGCInfo(true);

		if (ShouldPrintGCInfo()) {
			Print("===============================================");
			Print("Begin LoadCleanup");
			Print("===============================================");
			IncPadding();
		}

		auto timerStart = high_resolution_clock::now();
		orig_BSScript__IVMSaveLoadInterface__LoadCleanup(thisptr);
		auto timerEnd = high_resolution_clock::now();

		if (ShouldPrintGCInfo()) {
			DecPadding();
			Print("===============================================");
			Print("End LoadCleanup (%lld ms)", duration_cast<milliseconds>(timerEnd - timerStart).count());
			Print("===============================================");
		}

		EnableFullGCInfo(false);
	}

	//
	// Active objects
	//
	REL::Relocation<void __fastcall(RE::BSScript::Internal::VirtualMachine*, float, unsigned int*)> orig_BSScript__Internal__VirtualMachine__ProcessObjectCleanup;
	void __fastcall hk_BSScript__Internal__VirtualMachine__ProcessObjectCleanup(RE::BSScript::Internal::VirtualMachine* thisptr, float TimeBudget, unsigned int* origSize)
	{
		unsigned int nextToClean{};

		std::vector<std::string>names{};

		if (ShouldPrintGCInfo()) {
			nextToClean = thisptr->nextObjectToClean;

			for (auto&& scr : thisptr->objectsAwaitingCleanup)
				names.emplace_back(scr->GetTypeInfo()->GetName());
		}

		// side effect: *origSize = thisptr->objectsAwaitingCleanup.size();
		auto timerStart = high_resolution_clock::now();
		orig_BSScript__Internal__VirtualMachine__ProcessObjectCleanup(thisptr, TimeBudget, origSize);
		auto timerEnd = high_resolution_clock::now();

		//if (ShouldPrintGCInfo() && (*origSize != thisptr->objectsAwaitingCleanup.size() || nextToClean != thisptr->nextObjectToClean)) {
		if (ShouldPrintGCInfo() && names.size()) {
			Print("GC process%s objects (%f / %f ms)...Arr Size: %u -> %u, Cleanup: %u",
				TimeBudget ? "" : " full",
				duration_cast<microseconds>(timerEnd - timerStart).count() / 1000.f, TimeBudget,
				*origSize, thisptr->objectsAwaitingCleanup.size(), *origSize - thisptr->objectsAwaitingCleanup.size());

			for (auto&& s : names)
				Print("\t%s", s.c_str());
		}
	}

	//
	// Arrays
	//
	REL::Relocation<void __fastcall(RE::BSScript::Internal::VirtualMachine*, float)> orig_BSScript__Internal__VirtualMachine__ProcessArrayCleanup;
	void __fastcall hk_BSScript__Internal__VirtualMachine__ProcessArrayCleanup(RE::BSScript::Internal::VirtualMachine* thisptr, float TimeBudget)
	{
		unsigned int arr_size{}, next_idx{}, queue{};

		// &Size = &Arr + 0x10
		if (ShouldPrintGCInfo()) {
			arr_size = thisptr->arrays.size();
			next_idx = thisptr->nextArrayToClean;

			auto refcount = [](int** pArr) { return !*pArr || **pArr == 1; };
			for (auto&& a : thisptr->arrays)
				if (refcount(reinterpret_cast<int**>(&a)))
					queue++;
		}

		auto timerStart = high_resolution_clock::now();
		orig_BSScript__Internal__VirtualMachine__ProcessArrayCleanup(thisptr, TimeBudget);
		auto timerEnd = high_resolution_clock::now();


		if (ShouldPrintGCInfo() && queue ) {
			Print("GC process%s arrays (%f / %f ms)... Arr Size: %u -> %u, Queue: %u, Cleanup: %u",
				TimeBudget ? "" : " full",
				duration_cast<microseconds>(timerEnd - timerStart).count() / 1000.f, TimeBudget,
				arr_size, thisptr->arrays.size(), queue, arr_size - thisptr->arrays.size());
		}
	}

#define MAKE_HOOK(name, relid, instrlen) orig_##name = WriteHookWithReturn(relid, instrlen, hk_##name);

	void InstallHooks()
	{
		// Separate hook due to inlining
		//MAKE_HOOK(ProcessObjectCleanup,												REL::ID(1400223), 6);	//2755EE0 (GC_Obj_Impl)
		MAKE_HOOK(BSScript__Internal__VirtualMachine__ProcessObjectCleanup, (REL::ID)REL::RelocationID(98137, 104860), 5);  //2740F00 (GC_Obj)	SE:1263940 AE:1450110
		MAKE_HOOK(BSScript__Internal__VirtualMachine__ProcessArrayCleanup,  (REL::ID)REL::RelocationID(98136, 104859), 5);  //2740D60 (GC_Arr)	SE:1263840 AE:144FEF0
		MAKE_HOOK(BSScript__Internal__VirtualMachine__SaveCleanup,			(REL::ID)REL::RelocationID(98158, 104882), 5);  // SE:12658D0 AE:1451ED0
		MAKE_HOOK(BSScript__IVMSaveLoadInterface__LoadCleanup,				(REL::ID)REL::RelocationID(98110, 104833), 5);	// SE:125F6C0 AE:144BD80

		logger::info("Installed GC trace hooks");
	}

#undef MAKE_HOOK
}
