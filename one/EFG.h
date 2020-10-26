#pragma once
#include <cstdint>

using HWADG_ARCH_WORD = uint64_t;

#define HWADG_CF(GN) \
HWADG_ARCH_WORD HWADG_CFHandler_Args_##GN[8]; \
HWADG_ARCH_WORD HWADG_CFHandler_SaveDr_##GN[6]; \
LONG CALLBACK HWADG_CFHandler_##GN(/*__in*/  PEXCEPTION_POINTERS info) \
{ \
   if (info->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) { \
      PCONTEXT ctx = info->ContextRecord; \
      if (ctx->ContextFlags & CONTEXT_DEBUG_REGISTERS) { \
         for(int i = 0; i < 4; ++i) \
            if (ctx->Rip == HWADG_CFHandler_Args_##GN[i]) { \
               ctx->Rip = HWADG_CFHandler_Args_##GN[i+4]; \
               return EXCEPTION_CONTINUE_EXECUTION; \
            } \
         return EXCEPTION_CONTINUE_SEARCH; \
      } \
   } \
   return EXCEPTION_CONTINUE_SEARCH; \
} \
PVOID HWADG_CFInstHandler_##GN() \
{ \
   CONTEXT ctx; \
 \
   ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS; \
   if (GetThreadContext(GetCurrentThread(), &ctx)) \
   { \
      HWADG_CFHandler_SaveDr_##GN[0] = ctx.Dr0; \
      HWADG_CFHandler_SaveDr_##GN[1] = ctx.Dr1; \
      HWADG_CFHandler_SaveDr_##GN[2] = ctx.Dr2; \
      HWADG_CFHandler_SaveDr_##GN[3] = ctx.Dr3; \
      HWADG_CFHandler_SaveDr_##GN[4] = ctx.Dr6; \
      HWADG_CFHandler_SaveDr_##GN[5] = ctx.Dr7; \
   } \
   return AddVectoredExceptionHandler(1, &HWADG_CFHandler_##GN); \
} \
void HWADG_CFSetup_##GN(HWADG_ARCH_WORD f1, HWADG_ARCH_WORD f2, HWADG_ARCH_WORD f3, HWADG_ARCH_WORD f4, \
                         HWADG_ARCH_WORD f5, HWADG_ARCH_WORD f6, HWADG_ARCH_WORD f7, HWADG_ARCH_WORD f8) \
{ \
   CONTEXT ctx; \
   HWADG_CFHandler_Args_##GN[0] = f1; \
   HWADG_CFHandler_Args_##GN[1] = f2; \
   HWADG_CFHandler_Args_##GN[2] = f3; \
   HWADG_CFHandler_Args_##GN[3] = f4; \
   HWADG_CFHandler_Args_##GN[4] = f5; \
   HWADG_CFHandler_Args_##GN[5] = f6; \
   HWADG_CFHandler_Args_##GN[6] = f7; \
   HWADG_CFHandler_Args_##GN[7] = f8; \
 \
   ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS; \
   if (GetThreadContext(GetCurrentThread(), &ctx)) { \
      ctx.Dr0 = f1; \
      ctx.Dr1 = f2; \
      ctx.Dr2 = f3; \
      ctx.Dr3 = f4; \
      ctx.Dr7 = 0x155; \
      SetThreadContext(GetCurrentThread(), &ctx); \
   } \
} \
void HWADG_CFCleanup_##GN(PVOID exHandler) \
{ \
   CONTEXT ctx; \
   if (exHandler != NULL) \
      RemoveVectoredExceptionHandler(exHandler); \
   ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS; \
   if (GetThreadContext(GetCurrentThread(), &ctx)) { \
      ctx.Dr0 = HWADG_CFHandler_SaveDr_##GN[0]; \
      ctx.Dr1 = HWADG_CFHandler_SaveDr_##GN[1]; \
      ctx.Dr2 = HWADG_CFHandler_SaveDr_##GN[2]; \
      ctx.Dr3 = HWADG_CFHandler_SaveDr_##GN[3]; \
      ctx.Dr6 = HWADG_CFHandler_SaveDr_##GN[4]; \
      ctx.Dr7 = HWADG_CFHandler_SaveDr_##GN[5]; \
      SetThreadContext(GetCurrentThread(), &ctx); \
   } \
}