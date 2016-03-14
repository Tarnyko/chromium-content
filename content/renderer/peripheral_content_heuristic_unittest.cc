// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/peripheral_content_heuristic.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kSameOrigin[] = "http://same.com";
const char kOtherOrigin[] = "http://other.com";

}  // namespaces

TEST(PeripheralContentHeuristic, AllowSameOrigin) {
  EXPECT_EQ(
      PeripheralContentHeuristic::HEURISTIC_DECISION_ESSENTIAL_SAME_ORIGIN,
      PeripheralContentHeuristic::GetPeripheralStatus(
          std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
          url::Origin(GURL(kSameOrigin)), 100, 100));
  EXPECT_EQ(
      PeripheralContentHeuristic::HEURISTIC_DECISION_ESSENTIAL_SAME_ORIGIN,
      PeripheralContentHeuristic::GetPeripheralStatus(
          std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
          url::Origin(GURL(kSameOrigin)), 1000, 1000));
}

TEST(PeripheralContentHeuristic, DisallowCrossOriginUnlessLarge) {
  EXPECT_EQ(PeripheralContentHeuristic::HEURISTIC_DECISION_PERIPHERAL,
            PeripheralContentHeuristic::GetPeripheralStatus(
                std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
                url::Origin(GURL(kOtherOrigin)), 100, 100));
  EXPECT_EQ(
      PeripheralContentHeuristic::HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_BIG,
      PeripheralContentHeuristic::GetPeripheralStatus(
          std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
          url::Origin(GURL(kOtherOrigin)), 1000, 1000));
}

TEST(PeripheralContentHeuristic, AlwaysAllowTinyContent) {
  EXPECT_EQ(
      PeripheralContentHeuristic::HEURISTIC_DECISION_ESSENTIAL_SAME_ORIGIN,
      PeripheralContentHeuristic::GetPeripheralStatus(
          std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
          url::Origin(GURL(kSameOrigin)), 1, 1));
  EXPECT_EQ(PeripheralContentHeuristic::
                HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_TINY,
            PeripheralContentHeuristic::GetPeripheralStatus(
                std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
                url::Origin(GURL(kOtherOrigin)), 1, 1));
  EXPECT_EQ(PeripheralContentHeuristic::
                HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_TINY,
            PeripheralContentHeuristic::GetPeripheralStatus(
                std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
                url::Origin(GURL(kOtherOrigin)), 5, 5));
  EXPECT_EQ(PeripheralContentHeuristic::HEURISTIC_DECISION_PERIPHERAL,
            PeripheralContentHeuristic::GetPeripheralStatus(
                std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
                url::Origin(GURL(kOtherOrigin)), 10, 10));
}

TEST(PeripheralContentHeuristic, TemporaryOriginWhitelist) {
  EXPECT_EQ(PeripheralContentHeuristic::HEURISTIC_DECISION_PERIPHERAL,
            PeripheralContentHeuristic::GetPeripheralStatus(
                std::set<url::Origin>(), url::Origin(GURL(kSameOrigin)),
                url::Origin(GURL(kOtherOrigin)), 100, 100));

  std::set<url::Origin> whitelist;
  whitelist.insert(url::Origin(GURL(kOtherOrigin)));

  EXPECT_EQ(PeripheralContentHeuristic::
                HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_WHITELISTED,
            PeripheralContentHeuristic::GetPeripheralStatus(
                whitelist, url::Origin(GURL(kSameOrigin)),
                url::Origin(GURL(kOtherOrigin)), 100, 100));
}

}  // namespace content
