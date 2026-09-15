/* Binary128 coefficients and argument-reduction constants for bounded trigonometry. */
static const float128_t sine[] = {
    QUAD(0x0000000000000000, 0x3fff000000000000), QUAD(0x5555555555555555, 0xbffc555555555555),
    QUAD(0x1111111111111111, 0x3ff8111111111111), QUAD(0xa01a01a01a01a01a, 0xbff2a01a01a01a01),
    QUAD(0x38faac1c88e50017, 0x3fec71de3a556c73), QUAD(0x38fe747e4b837dc7, 0xbfe5ae64567f544e),
    QUAD(0x97ca38331d23af68, 0x3fde6124613a86d0), QUAD(0xf11d8656b0ee8cb0, 0xbfd6ae7f3e733b81),
    QUAD(0xa6b2605197771b00, 0x3fce952c77030ad4), QUAD(0x724ca1ec3b7b9675, 0xbfc62f49b4681415),
    QUAD(0x18bef146fcee6e45, 0x3fbd71b8ef6dcf57), QUAD(0x9d97b8704dd7f628, 0xbfb4761b41316381),
    QUAD(0x8d4e44a419776f11, 0x3fab3f3ccdd165fa), QUAD(0x320a9a18f15d4277, 0xbfa1d1ab1c2dccea),
    QUAD(0xd7abe30e7766f129, 0x3f98259f98b4358a), QUAD(0xc42e1ee46fa6bfc4, 0xbf8e434d2e783f5b),
    QUAD(0x1b5382cdffa97422, 0x3f843981254dd0d5), QUAD(0xf2833c7f5a7e0624, 0xbf7a0dc59c716d91)};
static const float128_t cosine[] = {
    QUAD(0x0000000000000000, 0x3fff000000000000), QUAD(0x0000000000000000, 0xbffe000000000000),
    QUAD(0x5555555555555555, 0x3ffa555555555555), QUAD(0x6c16c16c16c16c17, 0xbff56c16c16c16c1),
    QUAD(0xa01a01a01a01a01a, 0x3fefa01a01a01a01), QUAD(0xc72ef016d3ea6679, 0xbfe927e4fb7789f5),
    QUAD(0x7b544da987acfe85, 0x3fe21eed8eff8d89), QUAD(0xd20badf145dfa3e5, 0xbfda93974a8c07c9),
    QUAD(0xf11d8656b0ee8cb0, 0x3fd2ae7f3e733b81), QUAD(0x77bb004886a2c2ab, 0xbfca6827863b97d9),
    QUAD(0x507a9cad2bf8f0bb, 0x3fc1e542ba402022), QUAD(0x29450c90b7f338ec, 0xbfb90ce396db7f85),
    QUAD(0x7cca4b4067ca9d8a, 0x3faff2cf01972f57), QUAD(0x9a38f2050ba6b015, 0xbfa688e85fc6a4e5),
    QUAD(0xd373c5c51c354a8d, 0x3f9d0a18a2635085), QUAD(0xe60caded4c2989c5, 0xbf933932c5047d60),
    QUAD(0xc42e1ee46fa6bfc4, 0x3f89434d2e783f5b), QUAD(0xa13f8a2b4af9d6b7, 0xbf7f2710231c0fd7)};
static const float128_t pi_multiple[] = {
    QUAD(0x0000000000000000, 0x0000000000000000), QUAD(0x8469800000000000, 0x3fff921fb54442d1),
    QUAD(0x8469800000000000, 0x4000921fb54442d1), QUAD(0x234f200000000000, 0x40012d97c7f3321d),
    QUAD(0x8469800000000000, 0x4001921fb54442d1), QUAD(0xe583e00000000000, 0x4001f6a7a2955385),
    QUAD(0x234f200000000000, 0x40022d97c7f3321d)};
static const float128_t pi_ratio = QUAD(0x00000614638b68ba, 0x3fff000000000000);
static const extFloat80_t quadrant_boundary[] = {
    {.signif = UINT64_C(0xc90fdaa22168c234), .signExp = 0x3ffe},
    {.signif = UINT64_C(0x96cbe3f9990e91a7), .signExp = 0x4000},
    {.signif = UINT64_C(0xfb53d14aa9c2f2c1), .signExp = 0x4000},
    {.signif = UINT64_C(0xafeddf4ddd3ba9ee), .signExp = 0x4001},
    {.signif = UINT64_C(0xe231d5f66595da7b), .signExp = 0x4001},
    {.signif = UINT64_C(0x8a3ae64f76f80584), .signExp = 0x4002},
};
