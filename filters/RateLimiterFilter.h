/**
 *
 *  RateLimiterFilter.h
 *
 */

#pragma once

#include <drogon/HttpFilter.h>
using namespace drogon;


class RateLimiterFilter : public HttpFilter<RateLimiterFilter>
{
  public:
    RateLimiterFilter() {}
    void doFilter(const HttpRequestPtr &req,
                  FilterCallback &&fcb,
                  FilterChainCallback &&fccb) override;
};

