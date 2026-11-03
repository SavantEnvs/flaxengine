// Known-answer tests over the engine's own parsers, built with normal flags by build.sh.
// Every expected value is derived by hand from the format definition, not from engine output.
#include "Engine/Visject/Graph.h"
#include "Engine/Serialization/MemoryReadStream.h"
#include "Engine/Serialization/Json.h"
#include "Engine/Serialization/JsonTools.h"
#include "Engine/Utilities/Encryption.h"
#include "Engine/Utilities/TextProcessing.h"
#include "Engine/Core/Types/Guid.h"
#include "Engine/Core/Types/Version.h"
#include "Engine/Core/Types/Variant.h"
#include "Engine/Core/Collections/Dictionary.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int Passed = 0, Failed = 0;

#define CHECK(name, cond) do { if (cond) { Passed++; printf("PASS %s\n", name); } else { Failed++; printf("FAIL %s (line %d)\n", name, __LINE__); } } while (0)

struct Buf
{
    std::vector<unsigned char> b;
    Buf& u8(unsigned v) { b.push_back((unsigned char)v); return *this; }
    Buf& u16(unsigned v) { return u8(v).u8(v >> 8); }
    Buf& u32(uint32 v) { return u16(v & 0xffff).u16(v >> 16); }
    Buf& i32(int32 v) { return u32((uint32)v); }
    Buf& i64(int64 v) { return u32((uint32)(v & 0xffffffff)).u32((uint32)((uint64)v >> 32)); }
    Buf& f32(float v) { uint32 u; memcpy(&u, &v, 4); return u32(u); }
};

typedef Graph<GraphNode<GraphBox>, GraphBox, GraphParameter> VGraph;

static std::string Str(const Array<char>& a) { return std::string(a.Get(), a.Count()); }

static void TestBase64()
{
    const char* words[] = { "Man", "Ma", "M", "ManMa" };
    const char* enc[] = { "TWFu", "TWE=", "TQ==", "TWFuTWE=" };
    for (int i = 0; i < 4; i++)
    {
        Array<char> out;
        Encryption::Base64Encode((const byte*)words[i], (int32)strlen(words[i]), out);
        CHECK("base64-encode", Str(out) == enc[i]);
        Array<byte> dec;
        Encryption::Base64Decode(enc[i], (int32)strlen(enc[i]), dec);
        CHECK("base64-decode", dec.Count() == (int32)strlen(words[i]) && memcmp(dec.Get(), words[i], dec.Count()) == 0);
    }
    CHECK("base64-length", Encryption::Base64EncodeLength(5) == 8 && Encryption::Base64DecodeLength("TWFuTWE=", 8) == 5);
}

static void TestEncrypt()
{
    // data[i] = (data[i] ^ (byte)i) + (71 + 13 * i)
    byte data[3] = { 0x41, 0x42, 0x43 };
    Encryption::EncryptBytes(data, 3);
    CHECK("encrypt-kat", data[0] == 0x88 && data[1] == 0x97 && data[2] == 0xA2);
    Encryption::DecryptBytes(data, 3);
    CHECK("decrypt-roundtrip", data[0] == 0x41 && data[1] == 0x42 && data[2] == 0x43);
}

static void TestGuidVersion()
{
    Guid g;
    CHECK("guid-parse-n", !Guid::Parse(StringAnsiView("00112233445566778899aabbccddeeff"), g)
          && g.A == 0x00112233 && g.B == 0x44556677 && g.C == 0x8899aabb && g.D == 0xccddeeff);
    CHECK("guid-parse-bad", Guid::Parse(StringAnsiView("zz112233445566778899aabbccddeeff"), g));
    CHECK("guid-tostring", g.ToString() == TEXT("00112233445566778899aabbccddeeff"));

    Version v;
    CHECK("version-parse", !Version::Parse(String("1.2.3.4"), &v) && v.Major() == 1 && v.Minor() == 2 && v.Build() == 3 && v.Revision() == 4);
    CHECK("version-parse-short", !Version::Parse(String("5.6"), &v) && v.Major() == 5 && v.Minor() == 6);
}

static void TestStringUtils()
{
    int32 i = 0; uint32 h = 0; float f = 0;
    CHECK("parse-int", !StringUtils::Parse("-1234", 5u, &i) && i == -1234);
    CHECK("parse-hex", !StringUtils::ParseHex("FF", 2, &h) && h == 255);
    CHECK("parse-hex-bad", StringUtils::ParseHex("GG", 2, &h));
    CHECK("parse-float", !StringUtils::Parse("2.5", &f) && f == 2.5f);
}

static void TestJson()
{
    rapidjson_flax::Document doc;
    doc.Parse("{\"id\":\"0123456789abcdef0123456789abcdef\",\"n\":[1,2,{\"x\":\"fedcba9876543210fedcba9876543210\"}]}");
    CHECK("json-parse", !doc.HasParseError() && doc.IsObject());
    Guid id = JsonTools::GetGuid(doc["id"]);
    CHECK("json-guid", id.A == 0x01234567 && id.B == 0x89abcdef && id.C == 0x01234567 && id.D == 0x89abcdef);

    Dictionary<Guid, Guid> mapping;
    mapping[id] = Guid(0x11111111, 0x22222222, 0x33333333, 0x44444444);
    JsonTools::ChangeIds(doc, mapping);
    CHECK("json-changeids", strcmp(doc["id"].GetString(), "11111111222222223333333344444444") == 0);
    CHECK("json-changeids-untouched", strcmp(doc["n"][2]["x"].GetString(), "fedcba9876543210fedcba9876543210") == 0);

    rapidjson_flax::Document a, b;
    a.Parse("{\"p\":1}");
    b.Parse("{\"q\":2}");
    JsonTools::MergeDocuments(a, b);
    CHECK("json-merge", a.IsObject() && a["p"].GetInt() == 1 && a["q"].GetInt() == 2);
}

static void TestText()
{
    const char* src = "float4 foo bar;";
    TextProcessing reader(src, (int32)strlen(src));
    reader.Setup_HLSL();
    TextProcessing::Token t;
    const char* expect[] = { "float4", "foo", "bar" };
    int ok = 0;
    for (int i = 0; i < 3 && reader.CanRead(); i++)
    {
        reader.ReadToken(&t);
        if (t.Length == (int32)strlen(expect[i]) && memcmp(t.Start, expect[i], t.Length) == 0)
            ok++;
    }
    CHECK("text-tokens", ok == 3);

    const char* lines = "alpha\nbeta\ngamma";
    TextProcessing lr(lines, (int32)strlen(lines));
    lr.ReadLine(&t);
    bool a = t.Length == 5 && memcmp(t.Start, "alpha", 5) == 0;
    lr.ReadLine(&t);
    bool b = t.Length == 4 && memcmp(t.Start, "beta", 4) == 0;
    lr.ReadLine(&t);
    bool c = t.Length == 5 && memcmp(t.Start, "gamma", 5) == 0;
    CHECK("text-lines", a && b && c);
}

static void TestVariant()
{
    Buf bi; bi.u8(VariantType::Int).i32(0).i32(42);
    MemoryReadStream si(bi.b.data(), (uint32)bi.b.size());
    Variant vi; si.Read(vi);
    CHECK("variant-int", vi.Type.Type == VariantType::Int && vi.AsInt == 42);

    Buf bf; bf.u8(VariantType::Float).i32(0).f32(3.5f);
    MemoryReadStream sf(bf.b.data(), (uint32)bf.b.size());
    Variant vf; sf.Read(vf);
    CHECK("variant-float", vf.Type.Type == VariantType::Float && vf.AsFloat == 3.5f);

    Buf bs; bs.u8(VariantType::String).i32(0).i32(2).u16('h' ^ 0xFFF2).u16('i' ^ 0xFFF2);
    MemoryReadStream ss(bs.b.data(), (uint32)bs.b.size());
    Variant vs; ss.Read(vs);
    CHECK("variant-string", vs.Type.Type == VariantType::String && (StringView)vs == StringView(TEXT("hi")));

    Buf bb; bb.u8(VariantType::Bool).i32(0).u8(1);
    MemoryReadStream sb(bb.b.data(), (uint32)bb.b.size());
    Variant vb; sb.Read(vb);
    CHECK("variant-bool", vb.Type.Type == VariantType::Bool && vb.AsBool);
}

static void TestVisjectGraph()
{
    // Visject graph v7000: node 7 (type 0x00020001, values [Int 42], box 0 Float unconnected),
    // node 9 (type 5, box 1 Float connected to node 7 box 0).
    Buf g;
    g.i32(1963542358).u32(7000).i32(2).i32(0);
    g.u32(7).u32(0x00020001);
    g.u32(9).u32(5);
    // node 7
    g.i32(1).u8(VariantType::Int).i32(0).i32(42);
    g.u16(1).u8(0).u8(VariantType::Float).i32(0).u16(0);
    g.i32(0);
    // node 9
    g.i32(0);
    g.u16(1).u8(1).u8(VariantType::Float).i32(0).u16(1).u32(7).u8(0);
    g.i32(0);
    // graph meta + terminator
    g.i32(0);
    g.u8('\t');

    MemoryReadStream stream(g.b.data(), (uint32)g.b.size());
    VGraph graph;
    bool failed = graph.Load(&stream, true);
    CHECK("graph-load", !failed);
    CHECK("graph-nodes", graph.Nodes.Count() == 2 && graph.Nodes[0].ID == 7 && graph.Nodes[1].ID == 9);
    CHECK("graph-types", graph.Nodes[0].TypeID == 1 && graph.Nodes[0].GroupID == 2 && graph.Nodes[1].Type == 5);
    CHECK("graph-values", graph.Nodes[0].Values.Count() == 1 && graph.Nodes[0].Values[0].AsInt == 42);
    CHECK("graph-boxes", graph.Nodes[0].Boxes.Count() == 1 && graph.Nodes[0].Boxes[0].ID == 0
          && graph.Nodes[1].Boxes.Count() == 2 && graph.Nodes[1].Boxes[1].ID == 1);
    CHECK("graph-connection", graph.Nodes[1].Boxes[1].Connections.Count() == 1
          && graph.Nodes[1].Boxes[1].Connections[0] == &graph.Nodes[0].Boxes[0]);

    // Bad magic and a missing terminator are rejected.
    Buf bad = g; bad.b[0] ^= 1;
    MemoryReadStream badStream(bad.b.data(), (uint32)bad.b.size());
    VGraph badGraph;
    CHECK("graph-bad-magic", badGraph.Load(&badStream, true));
    Buf noEnd = g; noEnd.b.back() = 'x';
    MemoryReadStream noEndStream(noEnd.b.data(), (uint32)noEnd.b.size());
    VGraph noEndGraph;
    CHECK("graph-bad-terminator", noEndGraph.Load(&noEndStream, true));
}

int main()
{
    TestBase64();
    TestEncrypt();
    TestGuidVersion();
    TestStringUtils();
    TestJson();
    TestText();
    TestVariant();
    TestVisjectGraph();
    printf("SUMMARY passed=%d failed=%d\n", Passed, Failed);
    return Failed ? 1 : 0;
}
