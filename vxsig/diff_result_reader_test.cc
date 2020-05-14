// Copyright 2011-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "vxsig/diff_result_reader.h"

#include <cstddef>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/zynamics/binexport/util/filesystem.h"
#include "absl/status/status.h"
#include "third_party/zynamics/binexport/util/status_matchers.h"

using not_absl::IsOk;
using testing::Contains;
using testing::Eq;
using testing::IsTrue;

namespace security::vxsig {
namespace {

// Shorten access to placeholders.
namespace arg = ::std::placeholders;

// The fixture for testing DiffResultReader
class DiffResultReaderTest : public ::testing::Test {
 protected:
  enum {
    kNumFunctionMatches = 20,
    kNumBasicBlockMatches = 169,
    kNumInstructionMatches = 1049
  };

  DiffResultReaderTest()
      : expect_functions_(), expect_basic_blocks_(), expect_instructions_() {
    for (int i = 0; i < 2 * kNumFunctionMatches; i += 2) {
      expect_functions_.emplace(kFunctionMatches[i], kFunctionMatches[i + 1]);
    }

    for (int i = 0; i < 2 * kNumBasicBlockMatches; i += 2) {
      expect_basic_blocks_.emplace(kBasicBlockMatches[i],
                                   kBasicBlockMatches[i + 1]);
    }

    for (int i = 0; i < 2 * kNumInstructionMatches; i += 2) {
      expect_instructions_.emplace(kInstructionMatches[i],
                                   kInstructionMatches[i + 1]);
    }
  }

  // Manually verified function matches from sshd.korg_vs_sshd.trojan1.BinDiff,
  // exported with the SQLite console utility.
  static constexpr MemoryAddress kFunctionMatches[] = {
      0x00058360, 0x08095860, 0x000583a0, 0x08095890, 0x00058410, 0x080958f0,
      0x00058460, 0x08095930, 0x000584e0, 0x080959a0, 0x000585b0, 0x08095ea0,
      0x00058670, 0x08095f70, 0x00058750, 0x08096050, 0x00058830, 0x08096140,
      0x00058ca0, 0x08095a40, 0x00059150, 0x08096da0, 0x000594f0, 0x08097070,
      0x00059850, 0x080964d0, 0x00059940, 0x080965a0, 0x00059bb0, 0x08096760,
      0x00059e20, 0x08096920, 0x0005a170, 0x08096ad0, 0x0005a2c0, 0x0804cb78,
      0x0005a600, 0x08097670, 0x0005a940, 0x08097d80};

  // Like above, manually verified basic block matches from the same BinDiff
  // result file.
  static constexpr MemoryAddress kBasicBlockMatches[] = {
      0x00058360, 0x08095860, 0x00058391, 0x08095887, 0x00058396, 0x08095889,
      0x000583a0, 0x08095890, 0x000583c7, 0x080958b2, 0x000583e7, 0x080958c1,
      0x00058402, 0x080958e5, 0x00058410, 0x080958f0, 0x0005842e, 0x0809590b,
      0x0005843e, 0x08095917, 0x00058443, 0x08095919, 0x00058451, 0x08095925,
      0x00058460, 0x08095930, 0x000584ce, 0x08095989, 0x000584d4, 0x0809598f,
      0x000584e0, 0x080959a0, 0x00058538, 0x080959e8, 0x00058572, 0x08095a12,
      0x00058582, 0x08095a1e, 0x00058592, 0x08095a2a, 0x0005859d, 0x08095a39,
      0x000585b0, 0x08095ea0, 0x00058609, 0x08095f02, 0x0005860b, 0x08095f04,
      0x0005861b, 0x08095f10, 0x00058620, 0x08095f20, 0x0005862c, 0x08095f2a,
      0x00058638, 0x08095f35, 0x00058644, 0x08095f40, 0x0005864f, 0x08095f4b,
      0x00058661, 0x08095f5c, 0x00058670, 0x08095f70, 0x000586e0, 0x08095fed,
      0x000586e6, 0x08095ff4, 0x00058700, 0x08096005, 0x00058708, 0x0809600a,
      0x00058742, 0x0809603c, 0x00058749, 0x08096043, 0x00058750, 0x08096050,
      0x000587c2, 0x080960d1, 0x000587c8, 0x080960da, 0x000587e1, 0x080960f0,
      0x000587e9, 0x080960f7, 0x00058823, 0x08096129, 0x0005882a, 0x08096131,
      0x00058830, 0x08096140, 0x00058bad, 0x080964a8, 0x00058baf, 0x080964aa,
      0x00058bc6, 0x080964b6, 0x00058bd8, 0x0809622b, 0x00058c04, 0x0809623a,
      0x00058c34, 0x08096248, 0x00058c64, 0x08096351, 0x00058c99, 0x080964c1,
      0x00058ca0, 0x08095b91, 0x00058f82, 0x08095d5a, 0x00058fc6, 0x08095cbe,
      0x00058fe1, 0x08095cd5, 0x00059005, 0x08095c03, 0x00059020, 0x08095ca2,
      0x000590b7, 0x08095d97, 0x000590f6, 0x08095dd9, 0x0005913f, 0x08095e8f,
      0x00059150, 0x08096da0, 0x000591a6, 0x08096e68, 0x000591bf, 0x08096e97,
      0x000591e8, 0x08096f38, 0x0005921d, 0x08096fac, 0x00059237, 0x08096f0e,
      0x00059243, 0x08096f79, 0x00059260, 0x08096f9a, 0x00059292, 0x08096f88,
      0x000592a5, 0x08096f95, 0x000592d0, 0x08097022, 0x000592f8, 0x0809704a,
      0x0005930b, 0x08097056, 0x00059348, 0x08096f18, 0x00059355, 0x08097061,
      0x000594f0, 0x08097070, 0x00059536, 0x08097360, 0x00059561, 0x08097648,
      0x000595a6, 0x080973b9, 0x0005964b, 0x0809742a, 0x0005966a, 0x08097448,
      0x0005967f, 0x08097561, 0x000596a4, 0x08097509, 0x000596bb, 0x08097519,
      0x000596f8, 0x080973ab, 0x00059712, 0x08097275, 0x00059778, 0x08097528,
      0x0005979d, 0x08097555, 0x0005983a, 0x0809743e, 0x00059849, 0x0809766b,
      0x00059850, 0x080964d0, 0x000598c0, 0x08096528, 0x00059900, 0x0809656b,
      0x00059912, 0x0809657c, 0x00059927, 0x0809658f, 0x00059936, 0x08096597,
      0x00059940, 0x080965a0, 0x00059968, 0x080965c5, 0x00059af5, 0x0809668e,
      0x00059af7, 0x08096690, 0x00059b0e, 0x080966a0, 0x00059b20, 0x080966b0,
      0x00059ba2, 0x08096758, 0x00059bb0, 0x08096760, 0x00059bd8, 0x08096785,
      0x00059d65, 0x0809684e, 0x00059d67, 0x08096850, 0x00059d7e, 0x08096860,
      0x00059d90, 0x08096870, 0x00059e12, 0x08096918, 0x00059e20, 0x08096920,
      0x00059e48, 0x08096945, 0x00059fb5, 0x080969fe, 0x00059fb7, 0x08096a00,
      0x00059fce, 0x08096a10, 0x00059fe0, 0x08096a20, 0x0005a062, 0x08096ac8,
      0x0005a170, 0x08096ad0, 0x0005a1c4, 0x08096b22, 0x0005a21d, 0x08096b86,
      0x0005a278, 0x08096bf1, 0x0005a27a, 0x08096bf3, 0x0005a28d, 0x08096bff,
      0x0005a2a0, 0x08096c10, 0x0005a2ba, 0x08096c2e, 0x0005a5da, 0x0804cb78,
      0x0005a600, 0x08097670, 0x0005a707, 0x08097820, 0x0005a712, 0x08097835,
      0x0005a729, 0x08097858, 0x0005a740, 0x08097873, 0x0005a755, 0x08097a7f,
      0x0005a75c, 0x0809787c, 0x0005a784, 0x080978b8, 0x0005a786, 0x080978bd,
      0x0005a7b2, 0x080978da, 0x0005a7b6, 0x080978df, 0x0005a7c1, 0x080978ea,
      0x0005a7e0, 0x080978f8, 0x0005a7f4, 0x08097907, 0x0005a800, 0x08097910,
      0x0005a810, 0x08097928, 0x0005a823, 0x08097938, 0x0005a86e, 0x08097961,
      0x0005a891, 0x08097970, 0x0005a8de, 0x08097ba7, 0x0005a8ee, 0x08097bce,
      0x0005a8f6, 0x08097bdc, 0x0005a926, 0x08097c24, 0x0005a935, 0x08097d74,
      0x0005a940, 0x08097d80, 0x0005a9a4, 0x08097dd1, 0x0005a9b8, 0x08097de1,
      0x0005a9e0, 0x08097df0, 0x0005aa06, 0x08097e19, 0x0005aa0f, 0x08097e21,
      0x0005aa18, 0x08097e2e, 0x0005aa59, 0x08097e61, 0x0005aaa9, 0x08097e85,
      0x0005aab2, 0x08097e8e, 0x0005aae3, 0x08097ebe, 0x0005aaef, 0x08097ee6,
      0x0005ab0b, 0x0804cb78, 0x0005ab4e, 0x08097f62, 0x0005abb0, 0x08097f40,
      0x0005ac5e, 0x08097f5d};

  // Manually verified instruction matches.
  static constexpr MemoryAddress kInstructionMatches[] = {
      0x00058360, 0x08095863, 0x00058364, 0x08095866, 0x0005836d, 0x0809586c,
      0x00058372, 0x0809586f, 0x0005837b, 0x08095871, 0x00058381, 0x0809587b,
      0x00058386, 0x0809587e, 0x0005838f, 0x08095885, 0x00058395, 0x08095888,
      0x00058396, 0x08095889, 0x000583a0, 0x08095893, 0x000583a4, 0x08095896,
      0x000583ad, 0x0809589c, 0x000583b2, 0x0809589f, 0x000583b4, 0x080958a1,
      0x000583b9, 0x080958a6, 0x000583bf, 0x080958ab, 0x000583c5, 0x080958b0,
      0x000583c7, 0x080958b2, 0x000583cc, 0x080958b5, 0x000583d5, 0x080958bc,
      0x000583ec, 0x080958c1, 0x000583f3, 0x080958cd, 0x000583f6, 0x080958d5,
      0x000583fd, 0x080958e0, 0x00058402, 0x080958e5, 0x00058410, 0x080958f3,
      0x00058414, 0x080958f6, 0x0005841d, 0x080958fc, 0x00058422, 0x080958ff,
      0x00058424, 0x08095901, 0x0005842c, 0x08095909, 0x0005842e, 0x0809590b,
      0x00058433, 0x0809590e, 0x0005843c, 0x08095915, 0x00058442, 0x08095918,
      0x0005844c, 0x08095920, 0x00058451, 0x08095925, 0x00058460, 0x08095930,
      0x00058461, 0x08095931, 0x00058464, 0x08095937, 0x00058467, 0x0809593a,
      0x00058471, 0x08095940, 0x00058483, 0x08095945, 0x0005848c, 0x08095948,
      0x00058493, 0x08095950, 0x000584a0, 0x08095953, 0x000584a4, 0x08095957,
      0x000584a7, 0x0809595a, 0x000584aa, 0x0809595e, 0x000584ad, 0x0809596b,
      0x000584b6, 0x08095976, 0x000584bb, 0x0809597b, 0x000584c9, 0x0809597d,
      0x000584cc, 0x08095987, 0x000584ce, 0x08095989, 0x000584d2, 0x0809598c,
      0x000584d3, 0x0809598e, 0x000584d4, 0x0809598f, 0x000584e0, 0x080959a0,
      0x000584e4, 0x080959a1, 0x000584e7, 0x080959a3, 0x000584eb, 0x080959a4,
      0x000584ec, 0x080959a6, 0x000584ee, 0x080959a8, 0x000584f2, 0x080959ab,
      0x000584fb, 0x080959ae, 0x00058500, 0x080959b7, 0x00058508, 0x080959b9,
      0x0005850b, 0x080959bc, 0x0005850f, 0x080959c1, 0x00058513, 0x080959c4,
      0x00058516, 0x080959c7, 0x00058519, 0x080959ca, 0x0005851f, 0x080959cc,
      0x00058523, 0x080959d8, 0x00058526, 0x080959db, 0x00058529, 0x080959de,
      0x0005852d, 0x080959e1, 0x00058531, 0x080959e4, 0x00058538, 0x080959e8,
      0x00058544, 0x080959ea, 0x00058555, 0x080959f2, 0x0005855b, 0x080959f8,
      0x00058566, 0x080959fb, 0x00058569, 0x08095a06, 0x0005856d, 0x08095a0d,
      0x00058572, 0x08095a12, 0x00058578, 0x08095a15, 0x0005857d, 0x08095a1a,
      0x00058580, 0x08095a1c, 0x00058582, 0x08095a1e, 0x00058587, 0x08095a21,
      0x00058590, 0x08095a28, 0x00058592, 0x08095a2a, 0x00058596, 0x08095a2d,
      0x00058597, 0x08095a2e, 0x00058598, 0x08095a2f, 0x0005859a, 0x08095a30,
      0x0005859c, 0x08095a38, 0x0005859d, 0x08095a39, 0x000585b0, 0x08095ea4,
      0x000585bb, 0x08095ea7, 0x000585c4, 0x08095ead, 0x000585c9, 0x08095eb0,
      0x000585cb, 0x08095eb2, 0x000585d0, 0x08095eb9, 0x000585d5, 0x08095ec0,
      0x000585da, 0x08095ec7, 0x000585df, 0x08095edf, 0x000585e7, 0x08095ee6,
      0x000585ef, 0x08095eed, 0x000585f7, 0x08095ef5, 0x000585ff, 0x08095ef8,
      0x00058604, 0x08095efd, 0x00058607, 0x08095f00, 0x00058609, 0x08095f02,
      0x0005860b, 0x08095f04, 0x00058610, 0x08095f07, 0x00058619, 0x08095f0e,
      0x0005861b, 0x08095f10, 0x0005861f, 0x08095f18, 0x00058620, 0x08095f20,
      0x00058624, 0x08095f23, 0x0005862a, 0x08095f28, 0x0005862c, 0x08095f2a,
      0x00058630, 0x08095f2d, 0x00058636, 0x08095f33, 0x00058638, 0x08095f35,
      0x0005863c, 0x08095f38, 0x00058642, 0x08095f3e, 0x00058644, 0x08095f40,
      0x00058648, 0x08095f43, 0x0005864d, 0x08095f49, 0x0005864f, 0x08095f4b,
      0x00058652, 0x08095f4e, 0x00058658, 0x08095f51, 0x0005865d, 0x08095f58,
      0x0005865f, 0x08095f5a, 0x00058661, 0x08095f5c, 0x00058670, 0x08095f70,
      0x00058671, 0x08095f71, 0x0005867e, 0x08095f73, 0x00058682, 0x08095f75,
      0x00058686, 0x08095f78, 0x0005868f, 0x08095f7b, 0x00058694, 0x08095f84,
      0x00058696, 0x08095f86, 0x0005869b, 0x08095f91, 0x000586a0, 0x08095f98,
      0x000586a5, 0x08095f9f, 0x000586aa, 0x08095fa2, 0x000586b2, 0x08095fa6,
      0x000586ba, 0x08095fa9, 0x000586c2, 0x08095fb0, 0x000586ca, 0x08095fd0,
      0x000586d4, 0x08095fd5, 0x000586dc, 0x08095fe9, 0x000586de, 0x08095feb,
      0x000586e0, 0x08095fed, 0x000586e2, 0x08095fef, 0x000586e6, 0x08095ff4,
      0x000586ea, 0x08095ff6, 0x000586f3, 0x08095ff9, 0x000586f5, 0x08095ffe,
      0x000586f8, 0x08096001, 0x000586fe, 0x08096003, 0x00058700, 0x08096005,
      0x00058703, 0x08096007, 0x0005870f, 0x0809600a, 0x00058713, 0x0809600d,
      0x00058717, 0x08096010, 0x0005871c, 0x0809601c, 0x00058723, 0x08096020,
      0x00058728, 0x08096028, 0x0005872d, 0x0809602b, 0x00058732, 0x08096030,
      0x00058737, 0x08096033, 0x00058740, 0x0809603a, 0x00058742, 0x0809603c,
      0x00058746, 0x0809603f, 0x00058747, 0x08096040, 0x00058748, 0x08096042,
      0x00058749, 0x08096048, 0x00058750, 0x08096050, 0x00058751, 0x08096051,
      0x0005875a, 0x08096053, 0x0005875e, 0x08096056, 0x00058762, 0x0809605d,
      0x0005876b, 0x08096060, 0x00058770, 0x0809607a, 0x00058772, 0x0809607c,
      0x00058777, 0x08096083, 0x0005877c, 0x0809608a, 0x00058781, 0x08096091,
      0x00058786, 0x08096094, 0x0005878e, 0x08096098, 0x00058796, 0x0809609b,
      0x0005879e, 0x080960a2, 0x000587a6, 0x080960b4, 0x000587ad, 0x080960b9,
      0x000587b4, 0x080960bc, 0x000587bc, 0x080960bf, 0x000587c0, 0x080960cf,
      0x000587c2, 0x080960d3, 0x000587c4, 0x080960d8, 0x000587c8, 0x080960da,
      0x000587cd, 0x080960dd, 0x000587d6, 0x080960df, 0x000587d9, 0x080960e6,
      0x000587dc, 0x080960ec, 0x000587df, 0x080960ee, 0x000587e1, 0x080960f0,
      0x000587e4, 0x080960f2, 0x000587f0, 0x080960f7, 0x000587f4, 0x080960fa,
      0x000587f8, 0x080960fd, 0x000587fb, 0x08096101, 0x00058804, 0x0809610d,
      0x00058809, 0x08096115, 0x0005880e, 0x08096118, 0x00058813, 0x0809611d,
      0x00058818, 0x08096120, 0x00058821, 0x08096127, 0x00058823, 0x08096129,
      0x00058827, 0x0809612c, 0x00058828, 0x0809612d, 0x00058829, 0x08096130,
      0x0005882a, 0x08096131, 0x00058830, 0x08096140, 0x00058832, 0x08096141,
      0x00058837, 0x08096143, 0x0005883b, 0x08096144, 0x0005883c, 0x08096146,
      0x00058853, 0x0809614c, 0x00058856, 0x0809614f, 0x0005885f, 0x08096156,
      0x00058867, 0x08096159, 0x00058a3c, 0x0809615e, 0x00058a44, 0x08096168,
      0x00058a4c, 0x0809616f, 0x00058a54, 0x08096175, 0x00058a5c, 0x08096179,
      0x00058a64, 0x0809617d, 0x00058a6c, 0x08096181, 0x00058a74, 0x08096185,
      0x00058a7c, 0x08096189, 0x00058a84, 0x0809618d, 0x00058a8c, 0x08096191,
      0x00058a94, 0x08096195, 0x00058a9c, 0x08096199, 0x00058aa4, 0x0809619d,
      0x00058aac, 0x080961a1, 0x00058ab4, 0x080961a5, 0x00058abc, 0x080961a9,
      0x00058ac4, 0x080961ad, 0x00058acc, 0x080961b1, 0x00058ad4, 0x080961b5,
      0x00058adc, 0x080961b9, 0x00058ae4, 0x080961bd, 0x00058aec, 0x080961c1,
      0x00058af4, 0x080961c5, 0x00058afc, 0x080961c9, 0x00058b04, 0x080961cd,
      0x00058b0c, 0x080961d1, 0x00058b14, 0x080961d5, 0x00058b1c, 0x080961d9,
      0x00058b24, 0x080961dd, 0x00058b2c, 0x080961e1, 0x00058b34, 0x080961e5,
      0x00058b3c, 0x080961e9, 0x00058b44, 0x080961ed, 0x00058b4c, 0x080961f1,
      0x00058b54, 0x080961f5, 0x00058b5c, 0x080961f9, 0x00058b64, 0x080961fd,
      0x00058b6c, 0x08096201, 0x00058b74, 0x08096205, 0x00058b7c, 0x08096209,
      0x00058b84, 0x0809620d, 0x00058b8c, 0x08096211, 0x00058b99, 0x08096215,
      0x00058b9e, 0x08096219, 0x00058ba1, 0x0809621d, 0x00058bad, 0x080964a8,
      0x00058baf, 0x080964aa, 0x00058bb7, 0x080964ad, 0x00058bc0, 0x080964b4,
      0x00058bc6, 0x080964b6, 0x00058bcd, 0x080964bc, 0x00058bce, 0x080964bd,
      0x00058bcf, 0x080964be, 0x00058bd1, 0x080964c0, 0x00058c00, 0x0809622e,
      0x00058c24, 0x0809623d, 0x00058c3c, 0x08096248, 0x00058c41, 0x0809624f,
      0x00058c44, 0x08096253, 0x00058c4c, 0x08096335, 0x00058c51, 0x0809633d,
      0x00058c54, 0x08096341, 0x00058c57, 0x08096344, 0x00058c5c, 0x08096349,
      0x00058c5e, 0x0809634b, 0x00058c91, 0x08096351, 0x00058c94, 0x0809635d,
      0x00058c99, 0x080964c1, 0x00058cfd, 0x08095b91, 0x00058d05, 0x08095b97,
      0x00058eaa, 0x08095b9d, 0x00058eb2, 0x08095ba3, 0x00058eba, 0x08095ba6,
      0x00058ec2, 0x08095bad, 0x00058eca, 0x08095bb1, 0x00058ed2, 0x08095bb5,
      0x00058eda, 0x08095bbe, 0x00058ee2, 0x08095bc2, 0x00058eea, 0x08095bc6,
      0x00058ef2, 0x08095bca, 0x00058efa, 0x08095bce, 0x00058f02, 0x08095bd2,
      0x00058f0a, 0x08095bd6, 0x00058f12, 0x08095bda, 0x00058f1a, 0x08095bde,
      0x00058f22, 0x08095be2, 0x00058f2a, 0x08095be6, 0x00058f32, 0x08095bea,
      0x00058f3a, 0x08095bf2, 0x00058f42, 0x08095bf7, 0x00058f68, 0x08095bfd,
      0x00058f94, 0x08095d5a, 0x00058fa2, 0x08095d5d, 0x00058fc6, 0x08095cbe,
      0x00058fc9, 0x08095cc0, 0x00058fcd, 0x08095cc3, 0x00058fd4, 0x08095cc9,
      0x00058fd7, 0x08095ccb, 0x00058fda, 0x08095ccd, 0x00058fdf, 0x08095cd3,
      0x00058fe8, 0x08095cd5, 0x00059005, 0x08095c03, 0x00059008, 0x08095c0b,
      0x0005900c, 0x08095c14, 0x00059013, 0x08095c1a, 0x00059016, 0x08095c1c,
      0x00059019, 0x08095c1e, 0x0005901e, 0x08095c24, 0x0005902b, 0x08095ca2,
      0x00059071, 0x08095ca8, 0x000590bc, 0x08095d97, 0x000590c3, 0x08095d9d,
      0x000590c6, 0x08095da5, 0x000590cb, 0x08095daf, 0x000590d2, 0x08095db5,
      0x000590d7, 0x08095dbd, 0x000590da, 0x08095dc1, 0x000590de, 0x08095dc4,
      0x000590e3, 0x08095dc9, 0x000590eb, 0x08095dcc, 0x000590f4, 0x08095dd3,
      0x000590f6, 0x08095dd9, 0x000590fd, 0x08095ddf, 0x000590fe, 0x08095de0,
      0x000590ff, 0x08095de1, 0x00059101, 0x08095de2, 0x00059107, 0x08095de3,
      0x0005913f, 0x08095e8f, 0x00059150, 0x08096da1, 0x00059155, 0x08096dac,
      0x0005915a, 0x08096daf, 0x0005915c, 0x08096db5, 0x00059161, 0x08096dba,
      0x00059166, 0x08096dbe, 0x0005916b, 0x08096dc2, 0x00059170, 0x08096dc6,
      0x00059175, 0x08096dca, 0x00059181, 0x08096dce, 0x00059186, 0x08096dd2,
      0x0005918f, 0x08096dd6, 0x00059199, 0x08096e55, 0x0005919e, 0x08096e5a,
      0x000591a0, 0x08096e62, 0x000591aa, 0x08096e6c, 0x000591b5, 0x08096e72,
      0x000591bf, 0x08096e97, 0x000591d1, 0x08096ea1, 0x000591d5, 0x08096ef8,
      0x000591d7, 0x08096f00, 0x000591e8, 0x08096f38, 0x000591ea, 0x08096f3e,
      0x000591f2, 0x08096f47, 0x000591fa, 0x08096f4d, 0x00059202, 0x08096f55,
      0x00059206, 0x08096f58, 0x0005920b, 0x08096f5c, 0x00059210, 0x08096f66,
      0x00059215, 0x08096f75, 0x00059221, 0x08096fac, 0x00059223, 0x08096fd6,
      0x0005922c, 0x08096fdb, 0x00059237, 0x08096f0e, 0x0005923b, 0x08096f10,
      0x0005923d, 0x08096f12, 0x0005924c, 0x08096f7d, 0x00059256, 0x08096f80,
      0x0005925d, 0x08096f85, 0x0005927a, 0x08096f9a, 0x0005928c, 0x08096fa1,
      0x00059292, 0x08096f88, 0x000592a5, 0x08096f95, 0x000592d0, 0x08097022,
      0x000592d8, 0x08097028, 0x000592e0, 0x0809702e, 0x000592e9, 0x08097036,
      0x000592ee, 0x08097045, 0x000592f8, 0x0809704a, 0x00059300, 0x0809704d,
      0x00059309, 0x08097054, 0x0005933b, 0x08097056, 0x00059342, 0x08097060,
      0x00059348, 0x08096f1b, 0x00059355, 0x08097061, 0x000594f0, 0x08097071,
      0x000594f5, 0x0809707c, 0x000594fa, 0x0809707f, 0x000594fc, 0x08097082,
      0x00059501, 0x08097085, 0x00059506, 0x08097088, 0x00059509, 0x0809708e,
      0x0005950e, 0x08097095, 0x0005951a, 0x0809709a, 0x00059523, 0x080970a0,
      0x0005952d, 0x08097163, 0x00059536, 0x08097360, 0x00059539, 0x08097377,
      0x0005953e, 0x0809737c, 0x00059543, 0x08097382, 0x00059548, 0x0809738a,
      0x0005954d, 0x08097394, 0x00059559, 0x08097398, 0x00059561, 0x08097648,
      0x000595a6, 0x080973b9, 0x0005964b, 0x0809742a, 0x00059663, 0x08097436,
      0x0005966e, 0x0809744e, 0x0005967f, 0x08097561, 0x00059684, 0x08097567,
      0x000596a4, 0x08097509, 0x000596ac, 0x0809750c, 0x000596b5, 0x08097513,
      0x000596eb, 0x08097519, 0x000596f2, 0x08097523, 0x000596f8, 0x080973ab,
      0x0005970c, 0x080973b0, 0x00059714, 0x08097275, 0x00059716, 0x08097279,
      0x0005971d, 0x08097281, 0x0005971f, 0x0809728c, 0x00059778, 0x08097528,
      0x0005977c, 0x0809752e, 0x0005978b, 0x08097539, 0x0005978f, 0x0809754c,
      0x000597e9, 0x08097555, 0x000597f1, 0x0809755b, 0x0005983a, 0x0809743e,
      0x00059849, 0x0809766b, 0x00059850, 0x080964d0, 0x00059854, 0x080964d1,
      0x00059857, 0x080964d3, 0x00059859, 0x080964d4, 0x0005985e, 0x080964d5,
      0x0005985f, 0x080964d6, 0x00059862, 0x080964d8, 0x00059866, 0x080964db,
      0x0005986f, 0x080964de, 0x0005987c, 0x080964e4, 0x00059881, 0x080964e8,
      0x00059884, 0x080964eb, 0x0005988d, 0x08096501, 0x00059896, 0x08096504,
      0x00059899, 0x08096507, 0x0005989d, 0x0809650a, 0x000598a3, 0x0809650c,
      0x000598a7, 0x08096518, 0x000598aa, 0x0809651b, 0x000598ad, 0x0809651e,
      0x000598b1, 0x08096521, 0x000598b5, 0x08096524, 0x000598c9, 0x08096528,
      0x000598cc, 0x0809652c, 0x000598cf, 0x0809652f, 0x000598d5, 0x0809653a,
      0x000598da, 0x0809653f, 0x000598e1, 0x08096542, 0x000598e4, 0x08096546,
      0x000598e9, 0x08096551, 0x000598ee, 0x08096556, 0x000598f1, 0x08096558,
      0x000598f5, 0x0809655b, 0x000598f8, 0x0809655e, 0x000598fc, 0x08096562,
      0x00059900, 0x0809656b, 0x00059903, 0x0809656e, 0x00059905, 0x08096571,
      0x0005990a, 0x08096576, 0x0005990d, 0x08096578, 0x00059910, 0x0809657a,
      0x00059912, 0x0809657c, 0x00059920, 0x0809657f, 0x00059925, 0x0809658d,
      0x00059927, 0x0809658f, 0x0005992b, 0x08096592, 0x0005992c, 0x08096593,
      0x0005992d, 0x08096594, 0x0005992f, 0x08096595, 0x00059935, 0x08096596,
      0x00059936, 0x08096597, 0x00059940, 0x080965a0, 0x00059941, 0x080965a1,
      0x00059944, 0x080965a3, 0x00059945, 0x080965a6, 0x0005994c, 0x080965ac,
      0x00059955, 0x080965af, 0x0005995d, 0x080965b8, 0x0005995f, 0x080965bd,
      0x00059962, 0x080965bf, 0x00059968, 0x080965c5, 0x00059970, 0x080965c9,
      0x00059978, 0x080965cd, 0x00059980, 0x080965d1, 0x00059988, 0x080965d5,
      0x00059990, 0x080965d9, 0x00059998, 0x080965dd, 0x000599a0, 0x080965e1,
      0x000599a8, 0x080965e5, 0x000599b0, 0x080965e9, 0x000599b8, 0x080965ed,
      0x000599c0, 0x080965f1, 0x000599c8, 0x080965f5, 0x000599d0, 0x080965f9,
      0x000599d8, 0x080965fd, 0x000599e0, 0x08096601, 0x000599e8, 0x08096605,
      0x000599f0, 0x08096609, 0x000599f8, 0x0809660d, 0x00059a00, 0x08096611,
      0x00059a08, 0x08096615, 0x00059a10, 0x08096619, 0x00059a18, 0x0809661d,
      0x00059a20, 0x08096621, 0x00059a28, 0x08096625, 0x00059a30, 0x08096629,
      0x00059a38, 0x0809662d, 0x00059a40, 0x08096631, 0x00059a48, 0x08096635,
      0x00059a50, 0x08096639, 0x00059a58, 0x0809663d, 0x00059a60, 0x08096641,
      0x00059a68, 0x08096645, 0x00059a70, 0x08096649, 0x00059a78, 0x0809664d,
      0x00059a80, 0x08096651, 0x00059a88, 0x08096655, 0x00059a90, 0x08096659,
      0x00059a98, 0x0809665d, 0x00059aa0, 0x08096661, 0x00059aa8, 0x08096665,
      0x00059ab0, 0x08096669, 0x00059ab8, 0x0809666d, 0x00059ac0, 0x08096671,
      0x00059ac8, 0x08096675, 0x00059ad0, 0x08096679, 0x00059ad8, 0x0809667d,
      0x00059ae0, 0x08096681, 0x00059ae8, 0x08096685, 0x00059af0, 0x08096689,
      0x00059af3, 0x0809668c, 0x00059af5, 0x0809668e, 0x00059af7, 0x08096690,
      0x00059aff, 0x08096693, 0x00059b08, 0x0809669a, 0x00059b0e, 0x080966a0,
      0x00059b15, 0x080966a6, 0x00059b16, 0x080966a7, 0x00059b17, 0x080966aa,
      0x00059b20, 0x080966b0, 0x00059b24, 0x080966b3, 0x00059b32, 0x080966bb,
      0x00059b36, 0x080966c3, 0x00059b39, 0x080966c7, 0x00059b43, 0x080966cd,
      0x00059b53, 0x080966d0, 0x00059b68, 0x080966d3, 0x00059b6b, 0x080966d9,
      0x00059b6e, 0x080966dd, 0x00059ba2, 0x08096758, 0x00059bb0, 0x08096760,
      0x00059bb1, 0x08096761, 0x00059bb4, 0x08096763, 0x00059bb5, 0x08096766,
      0x00059bbc, 0x0809676c, 0x00059bc5, 0x0809676f, 0x00059bcd, 0x08096778,
      0x00059bcf, 0x0809677d, 0x00059bd2, 0x0809677f, 0x00059bd8, 0x08096785,
      0x00059be0, 0x08096789, 0x00059be8, 0x0809678d, 0x00059bf0, 0x08096791,
      0x00059bf8, 0x08096795, 0x00059c00, 0x08096799, 0x00059c08, 0x0809679d,
      0x00059c10, 0x080967a1, 0x00059c18, 0x080967a5, 0x00059c20, 0x080967a9,
      0x00059c28, 0x080967ad, 0x00059c30, 0x080967b1, 0x00059c38, 0x080967b5,
      0x00059c40, 0x080967b9, 0x00059c48, 0x080967bd, 0x00059c50, 0x080967c1,
      0x00059c58, 0x080967c5, 0x00059c60, 0x080967c9, 0x00059c68, 0x080967cd,
      0x00059c70, 0x080967d1, 0x00059c78, 0x080967d5, 0x00059c80, 0x080967d9,
      0x00059c88, 0x080967dd, 0x00059c90, 0x080967e1, 0x00059c98, 0x080967e5,
      0x00059ca0, 0x080967e9, 0x00059ca8, 0x080967ed, 0x00059cb0, 0x080967f1,
      0x00059cb8, 0x080967f5, 0x00059cc0, 0x080967f9, 0x00059cc8, 0x080967fd,
      0x00059cd0, 0x08096801, 0x00059cd8, 0x08096805, 0x00059ce0, 0x08096809,
      0x00059ce8, 0x0809680d, 0x00059cf0, 0x08096811, 0x00059cf8, 0x08096815,
      0x00059d00, 0x08096819, 0x00059d08, 0x0809681d, 0x00059d10, 0x08096821,
      0x00059d18, 0x08096825, 0x00059d20, 0x08096829, 0x00059d28, 0x0809682d,
      0x00059d30, 0x08096831, 0x00059d38, 0x08096835, 0x00059d40, 0x08096839,
      0x00059d48, 0x0809683d, 0x00059d50, 0x08096841, 0x00059d58, 0x08096845,
      0x00059d60, 0x08096849, 0x00059d63, 0x0809684c, 0x00059d65, 0x0809684e,
      0x00059d67, 0x08096850, 0x00059d6f, 0x08096853, 0x00059d78, 0x0809685a,
      0x00059d7e, 0x08096860, 0x00059d85, 0x08096866, 0x00059d86, 0x08096867,
      0x00059d87, 0x0809686a, 0x00059d90, 0x08096870, 0x00059d94, 0x08096873,
      0x00059da2, 0x0809687b, 0x00059da6, 0x08096883, 0x00059da9, 0x08096887,
      0x00059db3, 0x0809688d, 0x00059dc3, 0x08096890, 0x00059dd8, 0x08096893,
      0x00059ddb, 0x08096899, 0x00059dde, 0x0809689d, 0x00059e12, 0x08096918,
      0x00059e20, 0x08096920, 0x00059e21, 0x08096921, 0x00059e24, 0x08096923,
      0x00059e25, 0x08096926, 0x00059e2c, 0x0809692c, 0x00059e35, 0x0809692f,
      0x00059e3d, 0x08096938, 0x00059e3f, 0x0809693d, 0x00059e42, 0x0809693f,
      0x00059e48, 0x08096945, 0x00059e50, 0x08096949, 0x00059e58, 0x0809694d,
      0x00059e60, 0x08096951, 0x00059e68, 0x08096955, 0x00059e70, 0x08096959,
      0x00059e78, 0x0809695d, 0x00059e80, 0x08096961, 0x00059e88, 0x08096965,
      0x00059e90, 0x08096969, 0x00059e98, 0x0809696d, 0x00059ea0, 0x08096971,
      0x00059ea8, 0x08096975, 0x00059eb0, 0x08096979, 0x00059eb8, 0x0809697d,
      0x00059ec0, 0x08096981, 0x00059ec8, 0x08096985, 0x00059ed0, 0x08096989,
      0x00059ed8, 0x0809698d, 0x00059ee0, 0x08096991, 0x00059ee8, 0x08096995,
      0x00059ef0, 0x08096999, 0x00059ef8, 0x0809699d, 0x00059f00, 0x080969a1,
      0x00059f08, 0x080969a5, 0x00059f10, 0x080969a9, 0x00059f18, 0x080969ad,
      0x00059f20, 0x080969b1, 0x00059f28, 0x080969b5, 0x00059f30, 0x080969b9,
      0x00059f38, 0x080969bd, 0x00059f40, 0x080969c1, 0x00059f48, 0x080969c5,
      0x00059f50, 0x080969c9, 0x00059f58, 0x080969cd, 0x00059f60, 0x080969d1,
      0x00059f68, 0x080969d5, 0x00059f70, 0x080969d9, 0x00059f78, 0x080969dd,
      0x00059f80, 0x080969e1, 0x00059f88, 0x080969e5, 0x00059f90, 0x080969e9,
      0x00059f98, 0x080969ed, 0x00059fa0, 0x080969f1, 0x00059fa8, 0x080969f5,
      0x00059fb0, 0x080969f9, 0x00059fb3, 0x080969fc, 0x00059fb5, 0x080969fe,
      0x00059fb7, 0x08096a00, 0x00059fbf, 0x08096a03, 0x00059fc8, 0x08096a0a,
      0x00059fce, 0x08096a10, 0x00059fd5, 0x08096a16, 0x00059fd6, 0x08096a17,
      0x00059fd7, 0x08096a1a, 0x00059fe0, 0x08096a20, 0x00059fe4, 0x08096a23,
      0x00059ff2, 0x08096a2b, 0x00059ff6, 0x08096a33, 0x00059ff9, 0x08096a37,
      0x0005a003, 0x08096a3d, 0x0005a013, 0x08096a40, 0x0005a028, 0x08096a43,
      0x0005a02b, 0x08096a49, 0x0005a02e, 0x08096a4d, 0x0005a062, 0x08096ac8,
      0x0005a170, 0x08096ad0, 0x0005a172, 0x08096ad1, 0x0005a175, 0x08096ad3,
      0x0005a176, 0x08096ad4, 0x0005a177, 0x08096ad6, 0x0005a17e, 0x08096adf,
      0x0005a183, 0x08096ae2, 0x0005a188, 0x08096ae8, 0x0005a191, 0x08096aee,
      0x0005a199, 0x08096af1, 0x0005a19b, 0x08096af3, 0x0005a1a3, 0x08096af6,
      0x0005a1ab, 0x08096b0a, 0x0005a1ae, 0x08096b0d, 0x0005a1b3, 0x08096b12,
      0x0005a1b6, 0x08096b15, 0x0005a1bb, 0x08096b1a, 0x0005a1be, 0x08096b1c,
      0x0005a1c4, 0x08096b22, 0x0005a1c8, 0x08096b25, 0x0005a1cb, 0x08096b27,
      0x0005a1cd, 0x08096b29, 0x0005a1d0, 0x08096b32, 0x0005a1d5, 0x08096b37,
      0x0005a1d8, 0x08096b3a, 0x0005a1dd, 0x08096b3f, 0x0005a1e0, 0x08096b41,
      0x0005a1e8, 0x08096b47, 0x0005a1ed, 0x08096b4f, 0x0005a1f0, 0x08096b53,
      0x0005a1f3, 0x08096b56, 0x0005a1f6, 0x08096b5a, 0x0005a1fb, 0x08096b65,
      0x0005a1fe, 0x08096b68, 0x0005a203, 0x08096b6d, 0x0005a206, 0x08096b70,
      0x0005a20b, 0x08096b75, 0x0005a20e, 0x08096b78, 0x0005a213, 0x08096b7d,
      0x0005a218, 0x08096b82, 0x0005a21b, 0x08096b84, 0x0005a21d, 0x08096b86,
      0x0005a221, 0x08096b89, 0x0005a224, 0x08096b8b, 0x0005a226, 0x08096b8d,
      0x0005a229, 0x08096b96, 0x0005a22e, 0x08096b9b, 0x0005a231, 0x08096b9e,
      0x0005a236, 0x08096ba3, 0x0005a239, 0x08096ba5, 0x0005a241, 0x08096bab,
      0x0005a246, 0x08096bb3, 0x0005a249, 0x08096bb7, 0x0005a24c, 0x08096bba,
      0x0005a24f, 0x08096bbe, 0x0005a254, 0x08096bc9, 0x0005a257, 0x08096bcc,
      0x0005a25c, 0x08096bd1, 0x0005a25f, 0x08096bd4, 0x0005a264, 0x08096bd9,
      0x0005a267, 0x08096bdd, 0x0005a26a, 0x08096be0, 0x0005a26f, 0x08096be8,
      0x0005a274, 0x08096bed, 0x0005a276, 0x08096bef, 0x0005a278, 0x08096bf1,
      0x0005a27a, 0x08096bf3, 0x0005a282, 0x08096bf6, 0x0005a28b, 0x08096bfd,
      0x0005a28d, 0x08096bff, 0x0005a294, 0x08096c05, 0x0005a295, 0x08096c06,
      0x0005a296, 0x08096c07, 0x0005a298, 0x08096c09, 0x0005a2a0, 0x08096c10,
      0x0005a2a3, 0x08096c14, 0x0005a2a6, 0x08096c1c, 0x0005a2ab, 0x08096c1f,
      0x0005a2b0, 0x08096c24, 0x0005a2b3, 0x08096c27, 0x0005a2b8, 0x08096c2c,
      0x0005a2ba, 0x08096c30, 0x0005a5da, 0x0804cb78, 0x0005a600, 0x08097670,
      0x0005a606, 0x08097671, 0x0005a609, 0x08097673, 0x0005a60e, 0x08097674,
      0x0005a60f, 0x08097675, 0x0005a615, 0x08097676, 0x0005a619, 0x0809767c,
      0x0005a622, 0x0809767f, 0x0005a627, 0x08097689, 0x0005a629, 0x0809768b,
      0x0005a62e, 0x0809768e, 0x0005a633, 0x08097691, 0x0005a638, 0x08097695,
      0x0005a63d, 0x0809769b, 0x0005a642, 0x0809769f, 0x0005a647, 0x080976a3,
      0x0005a64c, 0x080976a7, 0x0005a651, 0x080976ab, 0x0005a656, 0x080976af,
      0x0005a65b, 0x080976b3, 0x0005a660, 0x080976b7, 0x0005a665, 0x080976bb,
      0x0005a66a, 0x080976bf, 0x0005a66f, 0x080976c3, 0x0005a674, 0x080976c7,
      0x0005a679, 0x080976cb, 0x0005a683, 0x080976cf, 0x0005a68a, 0x080976d3,
      0x0005a68f, 0x080977a2, 0x0005a694, 0x080977a7, 0x0005a697, 0x080977aa,
      0x0005a69a, 0x080977b0, 0x0005a69f, 0x080977b5, 0x0005a6a2, 0x080977bb,
      0x0005a6a5, 0x080977be, 0x0005a6ab, 0x080977c2, 0x0005a6b0, 0x080977c7,
      0x0005a6b5, 0x080977cd, 0x0005a6b8, 0x080977d5, 0x0005a6bb, 0x080977dc,
      0x0005a6c0, 0x080977e1, 0x0005a6c5, 0x080977e8, 0x0005a6ca, 0x080977ed,
      0x0005a6cf, 0x080977f4, 0x0005a6d2, 0x080977fa, 0x0005a6d7, 0x080977ff,
      0x0005a6dc, 0x08097806, 0x0005a6df, 0x0809780c, 0x0005a6e4, 0x08097813,
      0x0005a6e9, 0x08097816, 0x0005a6ec, 0x0809781c, 0x0005a707, 0x08097824,
      0x0005a709, 0x0809782d, 0x0005a70d, 0x08097830, 0x0005a710, 0x08097833,
      0x0005a712, 0x08097835, 0x0005a715, 0x0809783e, 0x0005a71a, 0x08097845,
      0x0005a71c, 0x08097847, 0x0005a71f, 0x08097849, 0x0005a72c, 0x08097858,
      0x0005a736, 0x0809785e, 0x0005a749, 0x08097873, 0x0005a74f, 0x08097876,
      0x0005a758, 0x08097a7f, 0x0005a75c, 0x0809787c, 0x0005a75f, 0x08097884,
      0x0005a762, 0x08097887, 0x0005a76b, 0x08097892, 0x0005a772, 0x08097897,
      0x0005a777, 0x0809789d, 0x0005a77c, 0x080978af, 0x0005a784, 0x080978bb,
      0x0005a790, 0x080978bd, 0x0005a794, 0x080978ca, 0x0005a799, 0x080978cd,
      0x0005a7ad, 0x080978d2, 0x0005a7b2, 0x080978da, 0x0005a7b6, 0x080978df,
      0x0005a7c9, 0x080978ea, 0x0005a7cc, 0x080978ec, 0x0005a7ce, 0x080978f3,
      0x0005a7e0, 0x080978f8, 0x0005a7e8, 0x080978fc, 0x0005a7eb, 0x08097900,
      0x0005a7ef, 0x08097903, 0x0005a7f2, 0x08097905, 0x0005a7f4, 0x08097907,
      0x0005a7f7, 0x0809790b, 0x0005a7f9, 0x0809790d, 0x0005a800, 0x08097910,
      0x0005a803, 0x08097913, 0x0005a80d, 0x08097921, 0x0005a810, 0x0809792e,
      0x0005a815, 0x08097932, 0x0005a81c, 0x08097935, 0x0005a823, 0x08097938,
      0x0005a82d, 0x0809793e, 0x0005a834, 0x0809794f, 0x0005a87f, 0x08097965,
      0x0005a889, 0x0809796b, 0x0005a89c, 0x08097970, 0x0005a8a5, 0x08097979,
      0x0005a8ac, 0x0809797c, 0x0005a8b3, 0x0809797f, 0x0005a8c2, 0x08097984,
      0x0005a8c7, 0x0809798c, 0x0005a8d0, 0x08097994, 0x0005a8d4, 0x0809799b,
      0x0005a8dc, 0x080979a9, 0x0005a8de, 0x08097ba7, 0x0005a8e3, 0x08097bc2,
      0x0005a8e6, 0x08097bc6, 0x0005a8e9, 0x08097bc9, 0x0005a8ee, 0x08097bce,
      0x0005a8f1, 0x08097bd7, 0x0005a8f6, 0x08097bdc, 0x0005a8f9, 0x08097be5,
      0x0005a8fe, 0x08097bea, 0x0005a901, 0x08097bf3, 0x0005a906, 0x08097bf8,
      0x0005a909, 0x08097c01, 0x0005a90e, 0x08097c0c, 0x0005a911, 0x08097c0f,
      0x0005a916, 0x08097c14, 0x0005a91b, 0x08097c17, 0x0005a924, 0x08097c1e,
      0x0005a926, 0x08097c24, 0x0005a92a, 0x08097c2a, 0x0005a92b, 0x08097c2b,
      0x0005a92c, 0x08097c2c, 0x0005a92e, 0x08097c2d, 0x0005a934, 0x08097c2e,
      0x0005a935, 0x08097d74, 0x0005a94d, 0x08097d81, 0x0005a952, 0x08097d86,
      0x0005a957, 0x08097d8d, 0x0005a95a, 0x08097d96, 0x0005a95f, 0x08097d99,
      0x0005a968, 0x08097d9c, 0x0005a96f, 0x08097d9f, 0x0005a976, 0x08097da2,
      0x0005a97b, 0x08097da5, 0x0005a97e, 0x08097da9, 0x0005a987, 0x08097db3,
      0x0005a98e, 0x08097dbb, 0x0005a996, 0x08097dc2, 0x0005a99b, 0x08097dc7,
      0x0005a99c, 0x08097dc8, 0x0005a9a0, 0x08097dcd, 0x0005a9a2, 0x08097dcf,
      0x0005a9a4, 0x08097dd1, 0x0005a9a9, 0x08097dd4, 0x0005a9b2, 0x08097ddb,
      0x0005a9b8, 0x08097de1, 0x0005a9bd, 0x08097de4, 0x0005a9c2, 0x08097de7,
      0x0005a9c7, 0x08097dea, 0x0005a9da, 0x08097ded, 0x0005a9e0, 0x08097df0,
      0x0005a9e5, 0x08097df7, 0x0005a9ea, 0x08097dfc, 0x0005a9ef, 0x08097e03,
      0x0005a9f2, 0x08097e05, 0x0005a9f7, 0x08097e0a, 0x0005a9fa, 0x08097e0d,
      0x0005a9fd, 0x08097e10, 0x0005aa02, 0x08097e15, 0x0005aa04, 0x08097e17,
      0x0005aa06, 0x08097e19, 0x0005aa09, 0x08097e1b, 0x0005aa0f, 0x08097e21,
      0x0005aa12, 0x08097e28, 0x0005aa18, 0x08097e2e, 0x0005aa1f, 0x08097e42,
      0x0005aa59, 0x08097e61, 0x0005aa66, 0x08097e7c, 0x0005aaa9, 0x08097e85,
      0x0005aaac, 0x08097e88, 0x0005aab2, 0x08097e8e, 0x0005aac5, 0x08097e96,
      0x0005aac9, 0x08097ea2, 0x0005aacc, 0x08097ea6, 0x0005aacf, 0x08097eaa,
      0x0005aad4, 0x08097eb2, 0x0005aad9, 0x08097eb5, 0x0005aade, 0x08097eb9,
      0x0005aae3, 0x08097eda, 0x0005aae7, 0x08097ede, 0x0005aaea, 0x08097ee1,
      0x0005aaef, 0x08097ee6, 0x0005aaf2, 0x08097ee9, 0x0005aaf7, 0x08097eee,
      0x0005aafc, 0x08097ef1, 0x0005ab05, 0x08097ef8, 0x0005ab30, 0x0804cb78,
      0x0005ab4e, 0x08097f68, 0x0005ab55, 0x08097f6d, 0x0005ab5a, 0x08097f70,
      0x0005ab5d, 0x08097f7f, 0x0005ab69, 0x08097f84, 0x0005ab6d, 0x08097f87,
      0x0005ab70, 0x08097f8b, 0x0005ab73, 0x08097f8f, 0x0005ab78, 0x08097f97,
      0x0005ab7d, 0x08097f9b, 0x0005ab82, 0x08097f9e, 0x0005ab87, 0x08097fa2,
      0x0005ab8c, 0x08097fa7, 0x0005abba, 0x08097f40, 0x0005abbd, 0x08097f44,
      0x0005abc2, 0x08097f48, 0x0005abc7, 0x08097f50, 0x0005abcc, 0x08097f53,
      0x0005abd1, 0x08097f58, 0x0005ac5e, 0x08097f5d};

  absl::flat_hash_set<MemoryAddressPair> expect_functions_;
  size_t num_function_matches_ = 0;
  absl::flat_hash_set<MemoryAddressPair> expect_basic_blocks_;
  size_t num_basic_block_matches_ = 0;
  absl::flat_hash_set<MemoryAddressPair> expect_instructions_;
  size_t num_instruction_matches_ = 0;
};

constexpr MemoryAddress DiffResultReaderTest::kFunctionMatches[];
constexpr MemoryAddress DiffResultReaderTest::kBasicBlockMatches[];
constexpr MemoryAddress DiffResultReaderTest::kInstructionMatches[];

void ReceiveFunctionMatches(
    const absl::flat_hash_set<MemoryAddressPair>& expect_functions,
    size_t* num_function_matches, const MemoryAddressPair& match) {
  ++*num_function_matches;
  EXPECT_THAT(expect_functions, Contains(match))
      << "Did not find match: 0x" << std::hex << match.first << ", 0x"
      << match.second;
}

void ReceiveBasicBlockMatches(
    const absl::flat_hash_set<MemoryAddressPair>& expect_basic_blocks,
    size_t* num_basic_block_matches, const MemoryAddressPair& match) {
  ++*num_basic_block_matches;
  EXPECT_THAT(expect_basic_blocks, Contains(match))
      << "Did not find match: 0x" << std::hex << match.first << ", 0x"
      << match.second;
}

void ReceiveInstructionMatches(
    const absl::flat_hash_set<MemoryAddressPair>& expect_instructions,
    size_t* num_instruction_matches, const MemoryAddressPair& match) {
  ++*num_instruction_matches;
  EXPECT_THAT(expect_instructions, Contains(match))
      << "Did not find match: 0x" << std::hex << match.first << ", 0x"
      << match.second;
}

TEST_F(DiffResultReaderTest, ParseNoMetadata) {
  std::string file_name = JoinPath(
      getenv("TEST_SRCDIR"),
      "com_google_vxsig/vxsig/testdata/sshd.korg_vs_sshd.trojan1.BinDiff");
  ASSERT_THAT(FileExists(file_name), IsTrue());

  // Parse the file, but don't retrieve metadata (last argument)
  EXPECT_THAT(
      ParseBinDiff(file_name,
                   std::bind(ReceiveFunctionMatches, expect_functions_,
                             &num_function_matches_, arg::_1),
                   std::bind(ReceiveBasicBlockMatches, expect_basic_blocks_,
                             &num_basic_block_matches_, arg::_1),
                   std::bind(ReceiveInstructionMatches, expect_instructions_,
                             &num_instruction_matches_, arg::_1),
                   /* metadata = */ nullptr),
      IsOk());
  EXPECT_THAT(num_function_matches_, Eq(kNumFunctionMatches));
  EXPECT_THAT(num_basic_block_matches_, Eq(kNumBasicBlockMatches));
  EXPECT_THAT(num_instruction_matches_, Eq(kNumInstructionMatches));
}

TEST_F(DiffResultReaderTest, ParseWithMetadata) {
  std::string file_name = JoinPath(
      getenv("TEST_SRCDIR"),
      "com_google_vxsig/vxsig/testdata/sshd.korg_vs_sshd.trojan1.BinDiff");
  ASSERT_THAT(FileExists(file_name), IsTrue());

  std::pair<FileMetaData, FileMetaData> meta;
  EXPECT_THAT(
      ParseBinDiff(file_name,
                   std::bind(ReceiveFunctionMatches, expect_functions_,
                             &num_function_matches_, arg::_1),
                   std::bind(ReceiveBasicBlockMatches, expect_basic_blocks_,
                             &num_basic_block_matches_, arg::_1),
                   std::bind(ReceiveInstructionMatches, expect_instructions_,
                             &num_instruction_matches_, arg::_1),
                   &meta),
      IsOk());
  EXPECT_THAT(num_function_matches_, Eq(kNumFunctionMatches));
  EXPECT_THAT(num_basic_block_matches_, Eq(kNumBasicBlockMatches));
  EXPECT_THAT(num_instruction_matches_, Eq(kNumInstructionMatches));

  // Check metadata
  EXPECT_THAT(meta.first.filename, Eq("sshd.korg"));
  EXPECT_THAT(meta.first.original_filename, Eq("sshd.korg.hera.zeus1"));
  EXPECT_THAT(meta.first.original_hash,
              Eq("F705209F5671A2F85336717908007769B9FAFE54"));

  EXPECT_THAT(meta.second.filename, Eq("sshd.trojan1"));
  EXPECT_THAT(meta.second.original_filename, Eq("sshd"));
  EXPECT_THAT(meta.second.original_hash,
              Eq("86781CF0DF581B166A9ACAE32373BEB465704B54"));
}

}  // namespace
}  // namespace security::vxsig
