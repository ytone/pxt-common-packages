#include "pxt.h"

// first reloc, then validate
// image as list of objects
// every loaded pointer has to have metadata! go and read first word
//   - can be valid double
//   - can be relocated pointer to a valid VTable
// reloc - local (within image, just add image pointer) or external (vtables only?)

// TODO reloc entries in the image
// TODO linked list of function stacks
// TODO optimze add/sub/etc
// TODO list of api number->name mapping in the image - resolved at load time
// TODO same for opcodes

// TODO look for patterns in output for combined instructions

namespace pxt {

//%
void op_stloc(FiberContext *ctx, unsigned arg) {
    ctx->sp[arg] = ctx->r0;
}

//%
void op_ldloc(FiberContext *ctx, unsigned arg) {
    ctx->r0 = ctx->sp[arg];
}

//%
void op_stcap(FiberContext *ctx, unsigned arg) {
    ctx->caps[arg] = ctx->r0;
}

//%
void op_ldcap(FiberContext *ctx, unsigned arg) {
    ctx->r0 = ctx->caps[arg];
}

//%
void op_stglb(FiberContext *ctx, unsigned arg) {
    globals[arg] = ctx->r0;
}

//%
void op_ldglb(FiberContext *ctx, unsigned arg) {
    ctx->r0 = globals[arg];
}

//%
void op_ldlit(FiberContext *ctx, unsigned arg) {
    ctx->r0 = ctx->img->pointerLiterals[arg];
}

//%
void op_ldnumber(FiberContext *ctx, unsigned arg) {
    ctx->r0 = (TValue)ctx->img->numberLiterals[arg];
}

//%
void op_jmp(FiberContext *ctx, unsigned arg) {
    ctx->pc += (int)arg;
}

//%
void op_jmpz(FiberContext *ctx, unsigned arg) {
    if (!toBoolQuick(ctx->r0))
        ctx->pc += (int)arg;
}

//%
void op_jmpnz(FiberContext *ctx, unsigned arg) {
    if (toBoolQuick(ctx->r0))
        ctx->pc += (int)arg;
}

//%
void op_callproc(FiberContext *ctx, unsigned arg) {
    *--ctx->sp = (TValue)(((ctx->pc - ctx->imgbase) << 8) | 2);
    ctx->pc = (uint16_t *)ctx->img->pointerLiterals[arg] + 4;
}

//%
void op_callind(FiberContext *ctx, unsigned arg) {
    auto fn = ctx->r0;
    if (!isPointer(fn))
        failedCast(fn);
    auto vt = getVTable((RefObject *)fn);
    if (vt->objectType != ValType::Function)
        failedCast(fn);
    
    if (arg != vt->reserved) {
        // TODO re-arrange the stack, so that the right number
        // of arguments is present
        failedCast(fn);
    }

    *--ctx->sp = (TValue)(((ctx->pc - ctx->imgbase) << 8) | 2);
    ctx->pc = (uint16_t *)fn + 4;
}

//%
void op_ret(FiberContext *ctx, unsigned arg) {
    unsigned numTmps = (arg & 0xf) | ((arg >> 8) & 0xff);
    unsigned numArgs = ((arg >> 4) & 0xf) | ((arg >> 16) & 0xff);
    ctx->sp += numTmps;
    auto retaddr = (intptr_t)*ctx->sp++;
    ctx->sp += numArgs;
    ctx->pc = ctx->imgbase + (retaddr >> 8);
}

//%
void op_pop(FiberContext *ctx, unsigned) {
    ctx->r0 = *ctx->sp++;
}

//%
void op_popmany(FiberContext *ctx, unsigned arg) {
    ctx->sp += arg;
}

//%
void op_pushmany(FiberContext *ctx, unsigned arg) {
    while (arg--) {
        *--ctx->sp = TAG_UNDEFINED;
    }
}

//%
void op_push(FiberContext *ctx, unsigned) {
    *--ctx->sp = ctx->r0;
}

//%
void op_ldspecial(FiberContext *ctx, unsigned arg) {
    ctx->r0 = (TValue)(uintptr_t)arg;
}

//%
void op_ldint(FiberContext *ctx, unsigned arg) {
    ctx->r0 = TAG_NUMBER(arg);
}

//%
void op_ldintneg(FiberContext *ctx, unsigned arg) {
    ctx->r0 = TAG_NUMBER(-(int)arg);
}

// To be generated by pxt
void call_getConfig(FiberContext *ctx) {
    int a0 = toInt(ctx->sp[0]);
    int a1 = toInt(ctx->r0); // last argument in r0

    int r = getConfig(a0, a1);

    ctx->r0 = fromInt(r);
    ctx->sp += 1;
}

void exec_loop(FiberContext *ctx) {
    auto opcodes = ctx->img->opcodes;
    while (ctx->pc) {
        uint16_t opcode = *ctx->pc++;
        if (opcode >> 15 == 0) {
            opcodes[opcode & OPCODE_BASE_MASK](ctx, opcode >> OPCODE_BASE_SIZE);
        } else if (opcode >> 14 == 0b10) {
            ((ApiFun)opcodes[opcode & 0x3fff])(ctx);
        } else {
            unsigned tmp = ((int32_t)opcode << (16 + 2)) >> (2 + OPCODE_BASE_SIZE);
            opcode = *ctx->pc++;
            opcodes[opcode & OPCODE_BASE_MASK](ctx, (opcode >> OPCODE_BASE_SIZE) + tmp);
        }
    }
}

} // namespace pxt