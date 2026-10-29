/* Exact memoization at the portable BIOS boundary. Original floating-point
 * operations remain the sole producer on a miss; no rounded lookup table,
 * changed angle convention or approximate trigonometry is introduced.
 * AffineTerms is synchronous on the game's single cooperative thread. */
struct PeaqAffineMemo {
    s16 sx, sy;
    u16 rotation, valid;
    s32 pa, pb, pc, pd;
};
static struct PeaqAffineMemo peaq_affine_memo[8];
static unsigned peaq_affine_next;

static void AffineTerms(s16 xScale, s16 yScale, u16 rotation,
                        s32 *pa, s32 *pb, s32 *pc, s32 *pd)
{
    for (unsigned i = 0; i < 8; ++i) {
        const struct PeaqAffineMemo *m = &peaq_affine_memo[i];
        if (m->valid && m->sx == xScale && m->sy == yScale && m->rotation == rotation) {
            *pa = m->pa; *pb = m->pb; *pc = m->pc; *pd = m->pd;
            return;
        }
    }
    float angle = rotation * (float)(2.0 * M_PI) / 256.0f;
    float s = sinf(angle) * 256.0f;
    float c = cosf(angle) * 256.0f;
    *pa = (s32)(c * xScale / 256.0f);
    *pb = (s32)(-s * yScale / 256.0f);
    *pc = (s32)(s * xScale / 256.0f);
    *pd = (s32)(c * yScale / 256.0f);
    struct PeaqAffineMemo *m = &peaq_affine_memo[peaq_affine_next];
    m->sx = xScale; m->sy = yScale; m->rotation = rotation;
    m->pa = *pa; m->pb = *pb; m->pc = *pc; m->pd = *pd;
    m->valid = 1;
    peaq_affine_next = (peaq_affine_next + 1) & 7u;
}
