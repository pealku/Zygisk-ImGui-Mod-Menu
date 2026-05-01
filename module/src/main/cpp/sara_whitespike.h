#ifndef SARA_WHITESPIKE_H
#define SARA_WHITESPIKE_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

struct SpikerSaraCtx {
    uintptr_t funcAddr     = 0;
    uintptr_t adrpAddr     = 0;
    uintptr_t ldrAddr      = 0;
    uint32_t adrpOrig      = 0;
    uint32_t ldrOrig       = 0;
    uint32_t adrpNew       = 0;
    uint32_t ldrNew        = 0;
    bool     found         = false;
    bool     enabled       = false;
};

static SpikerSaraCtx g_saraCtx;

static inline uint32_t encode_adrp_x8(uint64_t insn_addr, uint64_t target_page_aligned) {
    uint64_t pc_page = insn_addr & ~0xFFFULL;
    int64_t  imm     = ((int64_t)target_page_aligned - (int64_t)pc_page) >> 12;
    uint32_t immlo   = imm & 3;
    uint32_t immhi   = (imm >> 2) & 0x7FFFF;
    return (1u << 31) | (immlo << 29) | (0x10u << 24) | (immhi << 5) | 8;
}

static inline uint32_t encode_ldr_x8_uimm(uint64_t byte_offset) {
    uint32_t imm12 = (byte_offset >> 3) & 0xFFF;
    return (3u << 30) | (0xE5u << 22) | (imm12 << 10) | (8u << 5) | 8;
}

static inline int64_t decode_adrp_imm(uint32_t insn) {
    uint32_t immlo = (insn >> 29) & 3;
    uint32_t immhi = (insn >> 5) & 0x7FFFF;
    int64_t imm = ((int64_t)immhi << 2) | immlo;
    if (imm & (1 << 20)) imm |= ~((1LL << 21) - 1);
    return imm;
}

static inline int32_t decode_ldr_uimm12(uint32_t insn) {
    return (insn >> 10) & 0xFFF;
}

static inline bool is_adrp_x8(uint32_t insn) {
    return ((insn >> 24) & 0x9F) == 0x90 && (insn & 0x1F) == 8;
}

static inline bool is_ldr_x8_x8(uint32_t insn) {
    return (insn >> 24) == 0xF9 && ((insn >> 5) & 0x1F) == 8 && (insn & 0x1F) == 8;
}

static inline uint32_t mem_read32(uintptr_t addr) {
    uint32_t v;
    memcpy(&v, (const void*)addr, 4);
    return v;
}

static inline void mem_write32(uintptr_t addr, uint32_t val) {
    uintptr_t page = addr & ~(uintptr_t)(sysconf(_SC_PAGE_SIZE) - 1);
    size_t    len  = sysconf(_SC_PAGE_SIZE);
    mprotect((void*)page, len, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy((void*)addr, &val, 4);
    mprotect((void*)page, len, PROT_READ | PROT_EXEC);
}

static const uint8_t kSpikerSaraSig[] = {
    0xE8, 0x0F, 0x19, 0xFC,
    0xFD, 0x7B, 0x01, 0xA9,
    0xFC, 0x6F, 0x02, 0xA9
};
static const size_t kSigLen = sizeof(kSpikerSaraSig);

inline bool sara_find_and_patch(uintptr_t libyoyoBase, size_t libSize) {
    auto& ctx = g_saraCtx;
    uintptr_t scanEnd = libyoyoBase + libSize - kSigLen;
    std::vector<uintptr_t> candidates;

    for (uintptr_t ea = libyoyoBase; ea < scanEnd; ea += 4) {
        if (memcmp((const void*)ea, kSpikerSaraSig, kSigLen) == 0) {
            uint32_t adrpC = mem_read32(ea + 0xF64);
            if (is_adrp_x8(adrpC)) {
                candidates.push_back(ea);
            }
        }
    }

    if (candidates.empty()) return false;

    ctx.funcAddr = candidates[0];
    uintptr_t funcStart = ctx.funcAddr;
    uintptr_t funcEnd   = funcStart + 0x1C2C;

    struct SkillBlock {
        uintptr_t adrp_a, ldr_a;
        uint32_t adrp_v, ldr_v;
        uintptr_t ptr_ent;
    };
    std::vector<SkillBlock> blocks;

    for (uintptr_t ea = funcStart; ea < funcEnd - 20; ea += 4) {
        if (mem_read32(ea) != 0x52BF400A) continue;
        uintptr_t adrpA = ea - 4;
        uint32_t  adrpV = mem_read32(adrpA);
        if (!is_adrp_x8(adrpV)) continue;
        uintptr_t ldrA = ea + 8;
        uint32_t  ldrV = mem_read32(ldrA);
        if (!is_ldr_x8_x8(ldrV)) continue;

        int64_t imm     = decode_adrp_imm(adrpV);
        uintptr_t pcPg  = adrpA & ~0xFFFULL;
        uintptr_t targ  = pcPg + (imm << 12);
        uint32_t uimm   = decode_ldr_uimm12(ldrV);
        uintptr_t ptrE  = targ + (uimm << 3);

        SkillBlock sb;
        sb.adrp_a  = adrpA; sb.ldr_a = ldrA;
        sb.adrp_v  = adrpV; sb.ldr_v = ldrV;
        sb.ptr_ent = ptrE;
        blocks.push_back(sb);
    }

    if (blocks.empty()) return false;

    int idx = blocks.size() >= 4 ? 3 : (int)blocks.size() - 1;
    auto& blk = blocks[idx];

    uintptr_t wsPtrEnt = blk.ptr_ent + 0x228798;

    ctx.adrpAddr = blk.adrp_a; ctx.ldrAddr = blk.ldr_a;
    ctx.adrpOrig = blk.adrp_v; ctx.ldrOrig = blk.ldr_v;
    ctx.adrpNew  = encode_adrp_x8(blk.adrp_a, wsPtrEnt & ~0xFFFULL);
    ctx.ldrNew   = encode_ldr_x8_uimm(wsPtrEnt & 0xFFF);
    ctx.found    = true;
    return true;
}

inline void sara_apply() {
    if (!g_saraCtx.found) return;
    mem_write32(g_saraCtx.adrpAddr, g_saraCtx.adrpNew);
    mem_write32(g_saraCtx.ldrAddr,  g_saraCtx.ldrNew);
    g_saraCtx.enabled = true;
}

inline void sara_restore() {
    if (!g_saraCtx.found) return;
    mem_write32(g_saraCtx.adrpAddr, g_saraCtx.adrpOrig);
    mem_write32(g_saraCtx.ldrAddr,  g_saraCtx.ldrOrig);
    g_saraCtx.enabled = false;
}

#endif