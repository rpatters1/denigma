/*
 * Copyright (C) 2025, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <string>
#include <filesystem>
#include <iterator>
#include <fstream>

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"

using namespace denigma;

TEST(MnxLayouts, MultiInstrumentTest)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("multistaff_inst.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    nlohmann::json mnx;
    openJson(inputPath.parent_path() / "multistaff_inst.mnx", mnx);

    auto layouts = mnx["layouts"];
    ASSERT_TRUE(layouts.is_array());
    EXPECT_EQ(layouts.size(), 41);

    // Validate first layout (S0-ScrVw)
    EXPECT_EQ(layouts[0]["id"], "S0-ScrVw");
    ASSERT_EQ(layouts[0]["content"].size(), 5);

    /// Organ
    EXPECT_EQ(layouts[0]["content"][0]["type"], "group");
    ASSERT_EQ(layouts[0]["content"][0]["content"].size(), 2);
    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["type"], "group");
    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["label"], "Organ");
    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["symbol"], "brace");
    ASSERT_EQ(layouts[0]["content"][0]["content"][0]["content"].size(), 2);

    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["content"][0]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][0]["content"][0]["content"][0]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["content"][0]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["content"][0]["sources"][0]["staff"], 1);

    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["content"][1]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][0]["content"][0]["content"][1]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["content"][1]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[0]["content"][0]["content"][0]["content"][1]["sources"][0]["staff"], 2);

    EXPECT_EQ(layouts[0]["content"][0]["content"][1]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][0]["content"][1]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][0]["content"][1]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[0]["content"][0]["content"][1]["sources"][0]["staff"], 3);

    // Harpsichord 1
    EXPECT_EQ(layouts[0]["content"][1]["type"], "group");
    ASSERT_EQ(layouts[0]["content"][1]["content"].size(), 2);
    EXPECT_EQ(layouts[0]["content"][1]["type"], "group");
    EXPECT_EQ(layouts[0]["content"][1]["label"], "Harpsichord 1");
    EXPECT_EQ(layouts[0]["content"][1]["symbol"], "brace");
    ASSERT_EQ(layouts[0]["content"][1]["content"].size(), 2);

    EXPECT_EQ(layouts[0]["content"][1]["content"][0]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][1]["content"][0]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][1]["content"][0]["sources"][0]["part"], "P2");
    EXPECT_EQ(layouts[0]["content"][1]["content"][0]["sources"][0]["staff"], 1);
    EXPECT_EQ(layouts[0]["content"][1]["content"][0]["sources"][0]["label"], "RH");

    EXPECT_EQ(layouts[0]["content"][1]["content"][1]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][1]["content"][1]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][1]["content"][1]["sources"][0]["part"], "P2");
    EXPECT_EQ(layouts[0]["content"][1]["content"][1]["sources"][0]["staff"], 2);
    EXPECT_EQ(layouts[0]["content"][1]["content"][1]["sources"][0]["label"], "LH");

    // Harpsichord 2
    EXPECT_EQ(layouts[0]["content"][2]["type"], "group");
    ASSERT_EQ(layouts[0]["content"][2]["content"].size(), 2);
    EXPECT_EQ(layouts[0]["content"][2]["type"], "group");
    EXPECT_EQ(layouts[0]["content"][2]["label"], "Harpsichord 2");
    EXPECT_EQ(layouts[0]["content"][2]["symbol"], "brace");
    ASSERT_EQ(layouts[0]["content"][2]["content"].size(), 2);

    EXPECT_EQ(layouts[0]["content"][2]["content"][0]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][2]["content"][0]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][2]["content"][0]["sources"][0]["part"], "P3");
    EXPECT_EQ(layouts[0]["content"][2]["content"][0]["sources"][0]["staff"], 1);
    EXPECT_EQ(layouts[0]["content"][2]["content"][0]["sources"][0]["label"], "RH");

    EXPECT_EQ(layouts[0]["content"][2]["content"][1]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][2]["content"][1]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][2]["content"][1]["sources"][0]["part"], "P3");
    EXPECT_EQ(layouts[0]["content"][2]["content"][1]["sources"][0]["staff"], 2);
    EXPECT_EQ(layouts[0]["content"][2]["content"][1]["sources"][0]["label"], "LH");

    // other staves

    EXPECT_EQ(layouts[0]["content"][3]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][3]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][3]["sources"][0]["part"], "P4");
    EXPECT_EQ(layouts[0]["content"][3]["sources"][0]["labelref"], "name");

    EXPECT_EQ(layouts[0]["content"][4]["type"], "staff");
    ASSERT_EQ(layouts[0]["content"][4]["sources"].size(), 1);
    EXPECT_EQ(layouts[0]["content"][4]["sources"][0]["part"], "P5");
    EXPECT_EQ(layouts[0]["content"][4]["sources"][0]["labelref"], "name");

    // Validate second layout (S0-Sys1)
    EXPECT_EQ(layouts[1]["id"], "S0-Sys1");
    ASSERT_EQ(layouts[1]["content"].size(), 5);

    /// Organ
    EXPECT_EQ(layouts[1]["content"][0]["type"], "group");
    ASSERT_EQ(layouts[1]["content"][0]["content"].size(), 2);
    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["type"], "group");
    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["label"], "Organ");
    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["symbol"], "brace");
    ASSERT_EQ(layouts[1]["content"][0]["content"][0]["content"].size(), 2);

    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["content"][0]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["content"][0]["sources"][0]["staff"], 1);

    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["content"][1]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[1]["content"][0]["content"][0]["content"][1]["sources"][0]["staff"], 2);

    EXPECT_EQ(layouts[1]["content"][0]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[1]["content"][0]["content"][1]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[1]["content"][0]["content"][1]["sources"][0]["staff"], 3);

    /// Harpsichord 1
    EXPECT_EQ(layouts[1]["content"][1]["type"], "group");
    EXPECT_EQ(layouts[1]["content"][1]["label"], "Harpsichord 1");
    EXPECT_EQ(layouts[1]["content"][1]["symbol"], "brace");
    ASSERT_EQ(layouts[1]["content"][1]["content"].size(), 2);
    
    EXPECT_EQ(layouts[1]["content"][1]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[1]["content"][1]["content"][0]["sources"][0]["part"], "P2");
    EXPECT_EQ(layouts[1]["content"][1]["content"][0]["sources"][0]["staff"], 1);
    EXPECT_EQ(layouts[1]["content"][1]["content"][0]["sources"][0]["label"], "RH");
    
    EXPECT_EQ(layouts[1]["content"][1]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[1]["content"][1]["content"][1]["sources"][0]["part"], "P2");
    EXPECT_EQ(layouts[1]["content"][1]["content"][1]["sources"][0]["staff"], 2);
    EXPECT_EQ(layouts[1]["content"][1]["content"][1]["sources"][0]["label"], "LH");
    
    /// Harpsichord 2
    EXPECT_EQ(layouts[1]["content"][2]["type"], "group");
    EXPECT_EQ(layouts[1]["content"][2]["label"], "Harpsichord 2");
    EXPECT_EQ(layouts[1]["content"][2]["symbol"], "brace");
    ASSERT_EQ(layouts[1]["content"][2]["content"].size(), 2);
    
    EXPECT_EQ(layouts[1]["content"][2]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[1]["content"][2]["content"][0]["sources"][0]["part"], "P3");
    EXPECT_EQ(layouts[1]["content"][2]["content"][0]["sources"][0]["staff"], 1);
    EXPECT_EQ(layouts[1]["content"][2]["content"][0]["sources"][0]["label"], "RH");
    
    EXPECT_EQ(layouts[1]["content"][2]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[1]["content"][2]["content"][1]["sources"][0]["part"], "P3");
    EXPECT_EQ(layouts[1]["content"][2]["content"][1]["sources"][0]["staff"], 2);
    EXPECT_EQ(layouts[1]["content"][2]["content"][1]["sources"][0]["label"], "LH");

    // other staves

    EXPECT_EQ(layouts[1]["content"][3]["type"], "staff");
    ASSERT_EQ(layouts[1]["content"][3]["sources"].size(), 1);
    EXPECT_EQ(layouts[1]["content"][3]["sources"][0]["part"], "P4");
    EXPECT_EQ(layouts[1]["content"][3]["sources"][0]["labelref"], "name");

    EXPECT_EQ(layouts[1]["content"][4]["type"], "staff");
    ASSERT_EQ(layouts[1]["content"][4]["sources"].size(), 1);
    EXPECT_EQ(layouts[1]["content"][4]["sources"][0]["part"], "P5");
    EXPECT_EQ(layouts[1]["content"][4]["sources"][0]["labelref"], "name");

        // Validate third layout (S0-Sys2)
    EXPECT_EQ(layouts[2]["id"], "S0-Sys2");
    ASSERT_EQ(layouts[2]["content"].size(), 5);

    /// Organ
    EXPECT_EQ(layouts[2]["content"][0]["type"], "group");
    ASSERT_EQ(layouts[2]["content"][0]["content"].size(), 2);
    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["type"], "group");
    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["label"], "Org.");
    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["symbol"], "brace");
    ASSERT_EQ(layouts[2]["content"][0]["content"][0]["content"].size(), 2);

    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["content"][0]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["content"][0]["sources"][0]["staff"], 1);

    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["content"][1]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[2]["content"][0]["content"][0]["content"][1]["sources"][0]["staff"], 2);

    EXPECT_EQ(layouts[2]["content"][0]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][0]["content"][1]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[2]["content"][0]["content"][1]["sources"][0]["staff"], 3);

    /// Harpsichord 1
    EXPECT_EQ(layouts[2]["content"][1]["type"], "group");
    EXPECT_EQ(layouts[2]["content"][1]["label"], "Hpschd. 1");
    EXPECT_EQ(layouts[2]["content"][1]["symbol"], "brace");
    ASSERT_EQ(layouts[2]["content"][1]["content"].size(), 2);
    
    EXPECT_EQ(layouts[2]["content"][1]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][1]["content"][0]["sources"][0]["part"], "P2");
    EXPECT_EQ(layouts[2]["content"][1]["content"][0]["sources"][0]["staff"], 1);
    
    EXPECT_EQ(layouts[2]["content"][1]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][1]["content"][1]["sources"][0]["part"], "P2");
    EXPECT_EQ(layouts[2]["content"][1]["content"][1]["sources"][0]["staff"], 2);
    
    /// Harpsichord 2
    EXPECT_EQ(layouts[2]["content"][2]["type"], "group");
    EXPECT_EQ(layouts[2]["content"][2]["label"], "Hpschd. 2");
    EXPECT_EQ(layouts[2]["content"][2]["symbol"], "brace");
    ASSERT_EQ(layouts[2]["content"][2]["content"].size(), 2);
    
    EXPECT_EQ(layouts[2]["content"][2]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][2]["content"][0]["sources"][0]["part"], "P3");
    EXPECT_EQ(layouts[2]["content"][2]["content"][0]["sources"][0]["staff"], 1);
    EXPECT_EQ(layouts[2]["content"][2]["content"][0]["sources"][0]["label"], "fs");
    EXPECT_EQ(layouts[2]["content"][2]["content"][0]["sources"][0]["stem"], "up");
    
    EXPECT_EQ(layouts[2]["content"][2]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][2]["content"][1]["sources"][0]["part"], "P3");
    EXPECT_EQ(layouts[2]["content"][2]["content"][1]["sources"][0]["staff"], 2);
    
    /// Other Staves
    EXPECT_EQ(layouts[2]["content"][3]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][3]["sources"][0]["part"], "P4");
    EXPECT_EQ(layouts[2]["content"][3]["sources"][0]["label"], "fs 1");
    EXPECT_EQ(layouts[2]["content"][3]["sources"][0]["stem"], "up");
    
    EXPECT_EQ(layouts[2]["content"][4]["type"], "staff");
    EXPECT_EQ(layouts[2]["content"][4]["sources"][0]["part"], "P5");
    EXPECT_EQ(layouts[2]["content"][4]["sources"][0]["labelref"], "shortName");
    
    // Validate fourth layout (S0-Sys3)
    EXPECT_EQ(layouts[3]["id"], "S0-Sys3");
    ASSERT_EQ(layouts[3]["content"].size(), 5);

    /// Organ
    EXPECT_EQ(layouts[3]["content"][0]["type"], "group");
    ASSERT_EQ(layouts[3]["content"][0]["content"].size(), 2);
    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["type"], "group");
    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["label"], "Org.");
    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["symbol"], "brace");
    ASSERT_EQ(layouts[3]["content"][0]["content"][0]["content"].size(), 2);

    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["content"][0]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["content"][0]["sources"][0]["staff"], 1);

    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["content"][1]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[3]["content"][0]["content"][0]["content"][1]["sources"][0]["staff"], 2);

    EXPECT_EQ(layouts[3]["content"][0]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][0]["content"][1]["sources"][0]["part"], "P1");
    EXPECT_EQ(layouts[3]["content"][0]["content"][1]["sources"][0]["staff"], 3);

    /// Harpsichord 1
    EXPECT_EQ(layouts[3]["content"][1]["type"], "group");
    EXPECT_EQ(layouts[3]["content"][1]["label"], "Hpschd. 1");
    EXPECT_EQ(layouts[3]["content"][1]["symbol"], "brace");
    ASSERT_EQ(layouts[3]["content"][1]["content"].size(), 2);
    
    EXPECT_EQ(layouts[3]["content"][1]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][1]["content"][0]["sources"][0]["part"], "P2");
    EXPECT_EQ(layouts[3]["content"][1]["content"][0]["sources"][0]["staff"], 1);
    
    EXPECT_EQ(layouts[3]["content"][1]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][1]["content"][1]["sources"][0]["part"], "P2");
    EXPECT_EQ(layouts[3]["content"][1]["content"][1]["sources"][0]["staff"], 2);
    
    /// Harpsichord 2
    EXPECT_EQ(layouts[3]["content"][2]["type"], "group");
    EXPECT_EQ(layouts[3]["content"][2]["label"], "Hpschd. 2");
    EXPECT_EQ(layouts[3]["content"][2]["symbol"], "brace");
    ASSERT_EQ(layouts[3]["content"][2]["content"].size(), 2);
    
    EXPECT_EQ(layouts[3]["content"][2]["content"][0]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][2]["content"][0]["sources"][0]["part"], "P3");
    EXPECT_EQ(layouts[3]["content"][2]["content"][0]["sources"][0]["staff"], 1);
    
    EXPECT_EQ(layouts[3]["content"][2]["content"][1]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][2]["content"][1]["sources"][0]["part"], "P3");
    EXPECT_EQ(layouts[3]["content"][2]["content"][1]["sources"][0]["staff"], 2);
    
    /// Other Staves
    EXPECT_EQ(layouts[3]["content"][3]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][3]["sources"][0]["part"], "P4");
    EXPECT_EQ(layouts[3]["content"][3]["sources"][0]["labelref"], "shortName");
    
    EXPECT_EQ(layouts[3]["content"][4]["type"], "staff");
    EXPECT_EQ(layouts[3]["content"][4]["sources"][0]["part"], "P5");
    EXPECT_EQ(layouts[3]["content"][4]["sources"][0]["labelref"], "shortName");
}

TEST(MnxLayouts, Piano3StaffTest)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("piano3staff.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
        });

    auto doc = mnx::Document::create(inputPath.parent_path() / "piano3staff.mnx");
    ASSERT_TRUE( doc.layouts());
    auto layouts = doc.layouts().value();
    ASSERT_GE(layouts.size(), 6);

    EXPECT_EQ(layouts.size(), 6);
    ASSERT_EQ(layouts[0].content().size(), 3);
    ASSERT_EQ(layouts[0].content()[2].type(), "group");
    auto pianoGroup = layouts[0].content()[2].get<mnx::layout::Group>();
    ASSERT_EQ(pianoGroup.content().size(), 3);
    for (size_t i = 0; i < pianoGroup.content().size(); i++) {
        ASSERT_EQ(pianoGroup.content()[i].type(), "staff");
        auto staffInfo = pianoGroup.content()[i].get<mnx::layout::Staff>();
        ASSERT_EQ(staffInfo.sources().size(), 1);
        EXPECT_EQ(staffInfo.sources()[0].part(), "P3");
        EXPECT_EQ(staffInfo.sources()[0].staff(), i + 1) << "expected staff " << i + 1;
    }
}