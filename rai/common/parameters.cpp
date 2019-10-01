#include <rai/common/parameters.hpp>

// Credit price and reward rate take 360 days as one year and adjust quarterly
rai::Amount rai::CreditPrice(uint64_t timestamp)
{
    uint64_t constexpr quarter = 90 * 24 * 60 * 60;
    size_t constexpr max_quarters = 32;
    static const std::array<rai::Amount, max_quarters> prices = {
        1000 * mRAI, 1000 * mRAI, 1000 * mRAI, 1000 * mRAI,  // 1st year
        900  * mRAI, 800  * mRAI, 700  * mRAI, 600  * mRAI,  // 2nd year
        500  * mRAI, 400  * mRAI, 300  * mRAI, 200  * mRAI,  // 3rd year
        100  * mRAI, 90   * mRAI, 80  *  mRAI, 70   * mRAI,  // 4th year
        60   * mRAI, 50   * mRAI, 40  *  mRAI, 30   * mRAI,  // 5th year
        20   * mRAI, 10   * mRAI, 9   *  mRAI, 8    * mRAI,  // 6th year
        7    * mRAI, 6    * mRAI, 5   *  mRAI, 4    * mRAI,  // 7th year
        3    * mRAI, 2    * mRAI, 1   *  mRAI, 1    * mRAI,  // 8th year
    };

    if (timestamp < rai::EPOCH_TIMESTAMP)
    {
        return 0;
    }

    if (timestamp >= rai::EPOCH_TIMESTAMP + quarter * max_quarters)
    {
        return mRAI;
    }

    return prices[(timestamp - rai::EPOCH_TIMESTAMP) / quarter];
}

rai::Amount rai::RewardRate(uint64_t timestamp)
{
    uint64_t constexpr quarter = 90 * 24 * 60 * 60;
    size_t constexpr max_quarters = 16;
    static const std::array<rai::Amount, max_quarters> rates = {
        7800 * uRAI, 4600 * uRAI, 3200 * uRAI, 2500 * uRAI,  // 1st year
        1500 * uRAI, 1500 * uRAI, 1200 * uRAI, 1200 * uRAI,  // 2nd year
        620  * uRAI, 620  * uRAI, 620  * uRAI, 620  * uRAI,  // 3rd year
        270  * uRAI, 270  * uRAI, 270 *  uRAI, 270  * uRAI,  // 4th year
    };

    if (timestamp < rai::EPOCH_TIMESTAMP)
    {
        return 0;
    }

    if (timestamp >= rai::EPOCH_TIMESTAMP + quarter * max_quarters)
    {
        return 140 * uRAI;
    }

    return rates[(timestamp - rai::EPOCH_TIMESTAMP) / quarter];
}

rai::Amount rai::RewardAmount(const rai::Amount& balance, uint64_t begin,
                              uint64_t end)
{
    const rai::uint256_t  day_s(24 * 60 * 60);
    const rai::uint256_t  rai_s(rai::RAI);

    if (begin > end || begin < rai::EPOCH_TIMESTAMP)
    {
        return rai::Amount(0);
    }

    rai::Amount rate = rai::RewardRate(end);
    rai::uint256_t rate_s(rate.Number());
    rai::uint256_t balance_s(balance.Number());
    rai::uint256_t duration_s(end - begin);
    rai::uint256_t reward_s(balance_s * rate_s * duration_s / day_s / rai_s);

    return rai::Amount(static_cast<rai::uint128_t>(reward_s));
}

uint64_t rai::RewardTimestamp(uint64_t begin, uint64_t end)
{
    uint64_t constexpr day = 24 * 60 * 60;

    if (begin > end || begin < rai::EPOCH_TIMESTAMP)
    {
        return 0;
    }

    uint64_t result = begin + (end - begin) / 2 + day;
    if (end > result)
    {
        result = end;
    }
    
    return result;
}

uint16_t rai::MaxAllowedForks(uint64_t timestamp)
{
    uint16_t constexpr min_forks = 4;
    uint16_t constexpr max_forks = 256;
    uint64_t constexpr quarter = 90 * 24 * 60 * 60;

    if (timestamp < rai::EPOCH_TIMESTAMP)
    {
        return min_forks;
    }

    uint64_t forks = (timestamp - rai::EPOCH_TIMESTAMP) / quarter;
    if (forks < min_forks)
    {
        return min_forks;
    }
    if (forks > max_forks)
    {
        return max_forks;
    }
    return static_cast<uint16_t>(forks);
}