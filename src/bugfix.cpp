#include "bugfix.h"

namespace Bugfix
{
	bool InstallHooks()
	{
		/*

		SE_1_5_97:GC_Arr_14126D230
			.text: 000000014126D355 40 0F 94 C5		setz	bpl
			.text: 000000014126D359 40 84 ED		test	bpl,bpl
			.text: 000000014126D35C 75 0C			jnz		short loc_14126D36A -> 75 0A
			.text: 000000014126D35E 39 06			cmp		[rsi], eax
			.text: 000000014126D360 72 08			jb		short loc_14126D36A -> 75 06
			.text: 000000014126D362 FF C8			dec		eax
			.text: 000000014126D364 89 06			mov		[rsi], eax
			.text: 000000014126D366 EB 02			jmp		short loc_14126D36A -> 66 90
		; ---------------------------------------------------------------------------
			.text: 000000014126D368 FF C3			inc		ebx

		SE_1_5_97:GC_Obj_14126D3C0
			.text:000000014126D4E5 40 0F 94 C5      setz    bpl
			.text:000000014126D4E9 40 84 ED         test    bpl, bpl
			.text:000000014126D4EC 75 0C            jnz     short loc_14126D4FA -> 75 0A
			.text:000000014126D4EE 39 07            cmp     [rdi], eax
			.text:000000014126D4F0 72 08            jb      short loc_14126D4FA -> 75 06
			.text:000000014126D4F2 FF C8            dec     eax
			.text:000000014126D4F4 89 07            mov     [rdi], eax
			.text:000000014126D4F6 EB 02            jmp     short loc_14126D4FA -> 66 90
		; ---------------------------------------------------------------------------
			.text:000000014126D4F8 FF C3            inc     ebx

	  AE_1_6_1170:GC_Arr_14144FEF0
			.text:0000000141450032 40 0F 94 C6				setz    sil
			.text:0000000141450036 85 C0					test    eax, eax
			.text:0000000141450038 74 16					jz      short loc_141450050 -> 74 14
			.text:000000014145003A 41 39 86 00 94 00 00		cmp     [r14+9400h], eax
			.text:0000000141450041 72 0D					jb      short loc_141450050 -> 72 0B
			.text:0000000141450043 FF C8					dec     eax
			.text:0000000141450045 41 89 86 00 94 00 00		mov     [r14+9400h], eax
			.text:000000014145004C EB 02					jmp     short loc_141450050 -> 66 90
		; ---------------------------------------------------------------------------
			.text:000000014145004E FF C5					inc     ebp

	  AE_1_6_1170:GC_Obj_141450110
			.text:000000014145025C 40 0F 94 C6              setz    sil
			.text:0000000141450260 85 C0                    test    eax, eax
			.text:0000000141450262 74 16                    jz      short loc_14145027A -> 74 14
			.text:0000000141450264 41 39 86 D8 93 00 00     cmp     [r14+93D8h], eax
			.text:000000014145026B 72 0D                    jb      short loc_14145027A -> 72 0B
			.text:000000014145026D FF C8                    dec     eax
			.text:000000014145026F 41 89 86 D8 93 00 00     mov     [r14+93D8h], eax
			.text:0000000141450276 EB 02                    jmp     short loc_14145027A -> 66 90
		; ---------------------------------------------------------------------------
			.text:0000000141450278 FF C5                    inc     ebp
		*/
		constexpr struct patch16 { uint16_t offset, orig, patch; }
		patchArrSE[] =
		{
			{ 0x12C, 0x0C75, 0x0A75 },
			{ 0x130, 0x0872, 0x0675 },
			{ 0x136, 0x02EB, 0x9066 },
			{ 0x138, 0xC3FF, 0x0000 },
		},
		patchObjSE[] =
		{
			{ 0x12C, 0x0C75, 0x0A75 },
			{ 0x130, 0x0872, 0x0675 },
			{ 0x136, 0x02EB, 0x9066 },
			{ 0x138, 0xC3FF, 0x0000 },
		},
		patchArrAE[] = {
			{ 0x148, 0x1674, 0x1474 },
			{ 0x151, 0x0D72, 0x0B72 },
			{ 0x15C, 0x02EB, 0x9066 },
			{ 0x15E, 0xC5FF, 0x0000 },
		},
		patchObjAE[] = {
			{ 0x152, 0x1674, 0x1474 },
			{ 0x15B, 0x0D72, 0x0B72 },
			{ 0x166, 0x02EB, 0x9066 },
			{ 0x168, 0xC5FF, 0x0000 },
		};

		auto chk = +[](uintptr_t pos, const patch16 p[4])
		{
			for (int i = 0; i < 4; ++i)
				if (memcmp((void*)(pos + p[i].offset), &p[i].orig, 2))
					return logger::critical("Target AOB at {:#x}+{:#x} expect {:#x}->{:#x} mismatched!"sv, 
						pos, p[i].offset, p[i].orig, *(uint16_t*)(pos + p[i].offset)), false;
			return true;
		};
		auto apply = +[](uintptr_t pos, const patch16 p[4])
		{
			uintptr_t addr = pos + p->offset;
			uintptr_t len  = p[3].offset - p->offset + 2;

			DWORD oldProtect;
			VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);

			for (int i = 0; i < 3; ++i)
				memcpy((void*)(pos + p[i].offset), &p[i].patch, 2);

			VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
		};
		uintptr_t arr_pos = REL::RelocationID(98217, 104859).address();
		uintptr_t obj_pos = REL::RelocationID(98218, 104860).address();

		auto arr_patch = REL::Relocate(patchArrSE, patchArrAE);
		auto obj_patch = REL::Relocate(patchObjSE, patchObjAE);

		// check aob
		if (!chk(arr_pos, arr_patch) ||
			!chk(obj_pos, obj_patch))
			return false;

		// apply
		apply(arr_pos, arr_patch);
		apply(obj_pos, obj_patch);

		return logger::info("Installed bugfix hooks"), true;
	}
}
