// Copyright 2016 Aether authors. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//   http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================


#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include "../../../obj/obj.h"
#include <assert.h>
#define REQUIRE assert

std::set<int> erased;
int serializator_count = 0;

class A_00 : public aether::Obj {
public:
  AETHER_OBJ(A_00, aether::Obj);
  A_00() = default;
  A_00(Obj* parent, aether::Domain* domain) : Obj(parent, domain) {}
  virtual ~A_00() {
    erased.insert(i_);
  }
  template <typename T> void Serializator(T& s) {
    s & i_ & a_;
    serializator_count++;
  }
  int i_ = 123;
  std::vector<A_00::ptr> a_;
};

auto saver = [](const aether::Domain& domain, const aether::ObjId& obj_id, uint32_t class_id, const AETHER_OMSTREAM& os){
  std::filesystem::path dir = std::filesystem::path{"state"} / obj_id.ToString();
  std::filesystem::create_directories(dir);
  auto p = dir / std::to_string(class_id);
  std::ofstream f(p.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
  //   bool b = f.good();
  f.write((const char*)os.stream_.data(), os.stream_.size());
  std::cout << p.c_str() << " size: " << os.stream_.size() << "\n";
};

auto enumerator = [](const aether::Domain& domain, const aether::ObjId& obj_id){
  std::string path = obj_id.ToString();
  auto p = std::filesystem::path{"state"} / path;
  std::vector<uint32_t> classes;
  for(auto& f: std::filesystem::directory_iterator(p)) {
    classes.push_back(std::atoi(f.path().filename().c_str()));
  }
  return classes;
};

auto loader = [](const aether::Domain& domain, const aether::ObjId& obj_id, uint32_t class_id, AETHER_IMSTREAM& is){
  std::filesystem::path dir = std::filesystem::path{"state"} / obj_id.ToString();
  auto p = dir / std::to_string(class_id);
  std::ifstream f(p.c_str(), std::ios::in | std::ios::binary);
  if (!f.good()) return;
  f.seekg (0, f.end);
  std::streamoff length = f.tellg();
  f.seekg (0, f.beg);
  is.stream_.resize(length);
  f.read((char*)is.stream_.data(), length);
};

class V1 : public aether::Obj {
public:
  AETHER_OBJ(V1, aether::Obj);
  V1() = default;
  V1(Obj* parent, aether::Domain* domain) : Obj(parent, domain) {}
  int i = 11;
  template <typename T>
  void Serializator(T& s) {
    s & i;
  }
};

class V2 : public V1 {
public:
  AETHER_OBJ(V2, V1);
  V2() = default;
  V2(Obj* parent, aether::Domain* domain) : V1(parent, domain) {}
  float f = 2.2f;
  template <typename T>
  void Serializator(T& s) {
    s & f;
  }
};

class V3 : public V2 {
public:
  AETHER_OBJ(V3, V2);
  V3() = default;
  V3(Obj* parent, aether::Domain* domain) : V2(parent, domain) {}

  std::string s_{"text33"};
  template <typename T>
  void Serializator(T& s) {
    s & s_;
  }
};


void Versioning1() {
  aether::Domain domain{nullptr};
  domain.store_facility_ = saver;
  V1::ptr v1(domain.CreateObj(V1::kClassId, 1));
  v1->i = 111;
  V2::ptr v2(domain.CreateObj(V2::kClassId, 2));
  v2->i = 222;
  v2->f = 2.22f;
  V3::ptr v3(domain.CreateObj(V3::kClassId, 3));
  v3->i = 333;
  v3->f = 3.33f;
  v3->s_ = "text333";
  {
    // Upgrade serialized version: v1 -> v3, v2 -> v3, v3 -> v3
    aether::Domain domain{nullptr};
    domain.store_facility_ = saver;
    domain.load_facility_ = loader;
    domain.enumerate_facility_ = enumerator;
    AETHER_OMSTREAM os;
    os.custom_ = &domain;
    os << v3 << v2 << v1;
    // assert(os.stream_.size() == 103);
    aether::Domain domain1{nullptr};
    domain1.store_facility_ = saver;
    domain1.load_facility_ = loader;
    domain1.enumerate_facility_ = enumerator;
    AETHER_IMSTREAM is;
    is.custom_ = &domain1;
    is.stream_.insert(is.stream_.begin(), os.stream_.begin(), os.stream_.end());
    aether::Obj::ptr o1, o2, o3;
    is >> o3 >> o2 >> o1;
    REQUIRE(is.stream_.empty());
    V3::ptr v31 = o1;
    REQUIRE(v31);
    REQUIRE(v31->i == 111);
    REQUIRE(v31->f == 2.2f);
    REQUIRE(v31->s_ == "text33");
    V3::ptr v32 = o2;
    REQUIRE(v32);
    REQUIRE(v32->i == 222);
    REQUIRE(v32->f == 2.22f);
    REQUIRE(v32->s_ == "text33");
    V3::ptr v33 = o3;
    REQUIRE(v33);
    REQUIRE(v33->i == 333);
    REQUIRE(v33->f == 3.33f);
    REQUIRE(v33->s_ == "text333");
  }
  {
    // Downgrade serialized version: v3 -> v2, v2 -> v2
    aether::Domain domain{nullptr};
    domain.store_facility_ = saver;
    domain.load_facility_ = loader;
    domain.enumerate_facility_ = enumerator;
    AETHER_OMSTREAM os;
    os.custom_ = &domain;
    os << v3 << v2;
    //REQUIRE(os.stream_.size() == 87);
    aether::Domain domain1{nullptr};
    domain1.store_facility_ = saver;
    domain1.load_facility_ = loader;
    domain1.enumerate_facility_ = enumerator;
    AETHER_IMSTREAM is;
    is.custom_ = &domain1;
    is.stream_.insert(is.stream_.begin(), os.stream_.begin(), os.stream_.end());
    aether::Obj::ptr o2, o3;
    domain1.registry_.UnregisterClass(V3::kClassId);
    is >> o3 >> o2;
    REQUIRE(is.stream_.empty());
    V2::ptr v22 = o2;
    REQUIRE(v22);
    REQUIRE(v22->i == 222);
    REQUIRE(v22->f == 2.22f);
    V2::ptr v23 = o3;
    REQUIRE(v23);
    REQUIRE(v23->i == 333);
    REQUIRE(v23->f == 3.33f);
  }
  {
    // Upgrading by loading into the pointer
    aether::Domain domain{nullptr};
    domain.store_facility_ = saver;
    domain.load_facility_ = loader;
    domain.enumerate_facility_ = enumerator;
    AETHER_OMSTREAM os;
    os.custom_ = &domain;
    os << v2 << v1;
    // REQUIRE(os.stream_.size() == 48);
    aether::Domain domain1{nullptr};
    domain1.store_facility_ = saver;
    domain1.load_facility_ = loader;
    domain1.enumerate_facility_ = enumerator;
    AETHER_IMSTREAM is;
    is.custom_ = &domain1;
    is.stream_.insert(is.stream_.begin(), os.stream_.begin(), os.stream_.end());
    V2::ptr o2;
    V2::ptr o1;
    is >> o2 >> o1;
    REQUIRE(is.stream_.empty());
    REQUIRE(o2);
    REQUIRE(o2->i == 222);
    REQUIRE(o2->f == 2.22f);
    REQUIRE(o1);
    REQUIRE(o1->i == 111);
    REQUIRE(o1->f == 2.2f);
  }

  {
    // Downgrade serialized version: v3 -> v1, v2 -> v1
    aether::Domain domain{nullptr};
    domain.store_facility_ = saver;
    domain.load_facility_ = loader;
    domain.enumerate_facility_ = enumerator;
    AETHER_OMSTREAM os;
    os.custom_ = &domain;
    os << v3 << v2;
    // REQUIRE(os.stream_.size() == 87);
    aether::Domain domain1{nullptr};
    domain1.store_facility_ = saver;
    domain1.load_facility_ = loader;
    domain1.enumerate_facility_ = enumerator;
    AETHER_IMSTREAM is;
    is.custom_ = &domain1;
    is.stream_.insert(is.stream_.begin(), os.stream_.begin(), os.stream_.end());
    aether::Obj::ptr o2, o3;
    domain1.registry_.UnregisterClass(V2::kClassId);
    domain1.registry_.UnregisterClass(V3::kClassId);
    is >> o3 >> o2;
    REQUIRE(is.stream_.empty());
    V1::ptr v12 = o2;
    REQUIRE(v12);
    REQUIRE(v12->i == 222);
    V1::ptr v13 = o3;
    REQUIRE(v13);
    REQUIRE(v13->i == 333);
  }
}

void Pointers() {
  {
    // Both nullptrs
    A_00::ptr a1;
    A_00::ptr a2(nullptr);
    A_00::ptr a3(a1);
    A_00::ptr a4 = a1;
    a1 = nullptr;
    a1 = a2;
    a1 = a1;
    a3 = std::move(a2);
    a3 = std::move(a3);
    A_00::ptr a5(std::move(a3));
  }
  {
    aether::Domain domain{nullptr};
    A_00::ptr a0(domain.CreateObj(A_00::kClassId, 1));
  }
}

void SubgraphReleasing() {
  {
    aether::Domain domain{nullptr};
    A_00::ptr a(domain.CreateObj(A_00::kClassId, 1));
    a->i_ = 1;
    A_00::ptr b1{domain.CreateObj(A_00::kClassId, 2)};
    b1->i_ = 2;
    A_00::ptr b2{b1};

    A_00::ptr d1(domain.CreateObj(A_00::kClassId, 3));
    d1->i_ = 3;
    A_00::ptr d2{d1};
    A_00::ptr c{domain.CreateObj(A_00::kClassId, 4)};
    c->i_ = 4;

    a->a_.reserve(1);
    a->a_.push_back(std::move(b1));
    c->a_.reserve(2);
    c->a_.push_back(std::move(b2));
    c->a_.push_back(std::move(d2));
    d1->a_.reserve(1);
    d1->a_.push_back(std::move(c));

    erased.clear();
    a = nullptr;
    REQUIRE((erased == std::set{1}));
    erased.clear();
    d1 = nullptr;
    REQUIRE((erased == std::set{2, 3, 4}));
  }
  {
    aether::Domain domain{nullptr};
    A_00::ptr a(domain.CreateObj(A_00::kClassId, 1));
    a->i_ = 1;
    A_00::ptr b1{domain.CreateObj(A_00::kClassId, 2)};
    b1->i_ = 2;
    A_00::ptr b2{b1};

    A_00::ptr d1(domain.CreateObj(A_00::kClassId, 3));
    d1->i_ = 3;
    A_00::ptr d2{d1};
    A_00::ptr c{domain.CreateObj(A_00::kClassId, 4)};
    c->i_ = 4;

    a->a_.reserve(1);
    a->a_.push_back(std::move(b1));
    c->a_.reserve(2);
    c->a_.push_back(std::move(b2));
    c->a_.push_back(std::move(d2));
    d1->a_.reserve(1);
    d1->a_.push_back(std::move(c));

    erased.clear();
    d1 = nullptr;
    REQUIRE((erased == std::set{3, 4}));
    erased.clear();
    a = nullptr;
    REQUIRE((erased == std::set{1, 2}));
  }
  {
    aether::Domain domain{nullptr};
    A_00::ptr a1(domain.CreateObj(A_00::kClassId, 1));
    a1->i_ = 1;
    A_00::ptr a2{a1};
    A_00::ptr b{domain.CreateObj(A_00::kClassId, 2)};
    b->i_ = 2;

    b->a_.reserve(1);
    b->a_.push_back(std::move(a2));
    a1->a_.reserve(1);
    a1->a_.push_back(std::move(b));

    erased.clear();
    a1 = nullptr;
    REQUIRE((erased == std::set{1, 2}));
  }
  {
    aether::Domain domain{nullptr};
    A_00::ptr a1(domain.CreateObj(A_00::kClassId, 1));
    a1->i_ = 1;
    A_00::ptr a2{a1};
    A_00::ptr b{domain.CreateObj(A_00::kClassId, 2)};
    b->i_ = 2;
    A_00::ptr c1(domain.CreateObj(A_00::kClassId, 3));
    c1->i_ = 3;
    A_00::ptr c2{c1};

    b->a_.reserve(1);
    b->a_.push_back(std::move(a2));
    a1->a_.reserve(1);
    a1->a_.push_back(std::move(c2));
    c1->a_.reserve(1);
    c1->a_.push_back(std::move(b));

    erased.clear();
    a1 = nullptr;
    REQUIRE(erased.empty());
    erased.clear();
    c1 = nullptr;
    REQUIRE((erased == std::set{1, 2, 3}));
  }

  {
    aether::Domain domain{nullptr};
    A_00::ptr a(domain.CreateObj(A_00::kClassId, 1));
    a->i_ = 1;
    A_00::ptr b1{domain.CreateObj(A_00::kClassId, 2)};
    b1->i_ = 2;
    A_00::ptr b2{b1};
    A_00::ptr b3{b1};
    A_00::ptr c(domain.CreateObj(A_00::kClassId, 3));
    c->i_ = 3;

    a->a_.reserve(1);
    a->a_.push_back(std::move(b1));
    c->a_.reserve(1);
    c->a_.push_back(std::move(b2));
    b3->a_.reserve(1);
    b3->a_.push_back(std::move(c));

    erased.clear();
    a = nullptr;
    REQUIRE((erased == std::set{1}));
    erased.clear();
    b3 = nullptr;
    REQUIRE((erased == std::set{2, 3}));
  }
  {
    aether::Domain domain{nullptr};
    A_00::ptr a1(domain.CreateObj(A_00::kClassId, 1));
    a1->i_ = 1;
    A_00::ptr a2{a1};
    A_00::ptr b1{domain.CreateObj(A_00::kClassId, 2)};
    b1->i_ = 2;
    A_00::ptr b2{b1};
    A_00::ptr c(domain.CreateObj(A_00::kClassId, 3));
    c->i_ = 3;
    
    a1->a_.reserve(1);
    a1->a_.push_back(std::move(b1));
    c->a_.reserve(2);
    c->a_.push_back(std::move(a2));
    c->a_.push_back(std::move(b2));

    erased.clear();
    c.Release();
    REQUIRE((erased == std::set{3}));
    erased.clear();
    a1 = nullptr;
    REQUIRE((erased == std::set{1, 2}));
  }
  {
    aether::Domain domain{nullptr};
    A_00::ptr a1(domain.CreateObj(A_00::kClassId, 1));
    a1->i_ = 1;
    A_00::ptr a2{a1};
    A_00::ptr b1{domain.CreateObj(A_00::kClassId, 2)};
    b1->i_ = 2;
    A_00::ptr b2{b1};
    A_00::ptr c(domain.CreateObj(A_00::kClassId, 3));
    c->i_ = 3;
    A_00::ptr d(domain.CreateObj(A_00::kClassId, 4));
    d->i_ = 4;

    a1->a_.reserve(1);
    a1->a_.push_back(std::move(b1));
    c->a_.reserve(2);
    c->a_.push_back(std::move(b2));
    d->a_.reserve(1);
    d->a_.push_back(std::move(a2));
    c->a_.push_back(std::move(d));

    erased.clear();
    c.Release();
    REQUIRE((erased == std::set{3, 4}));
    erased.clear();
    a1 = nullptr;
    REQUIRE((erased == std::set{1, 2}));
  }
}

void Serialization() {
  {
    std::filesystem::remove_all("state");
    aether::Domain domain{nullptr};
    domain.store_facility_ = saver;
    A_00::ptr root(domain.CreateObj(A_00::kClassId, 666));
    root->i_ = 345;
    root.Serialize();
  }
  {
    aether::Domain domain{nullptr};
    domain.load_facility_ = loader;
    domain.enumerate_facility_ = enumerator;
    A_00::ptr root;
    root.SetId(666);
    root.Load(&domain);
    int sdf=0;
  }
  // Serialize / Load / Unload
  {
    std::filesystem::remove_all("state");
    erased.clear();
    {
      aether::Domain domain{nullptr};
      domain.store_facility_ = saver;
      A_00::ptr root(domain.CreateObj(A_00::kClassId, 666));
      root->i_ = 666;
      root->a_.reserve(3);
      auto b1 = root->a_.emplace_back(domain.CreateObj(A_00::kClassId, 1));
      b1->i_ = 1;
      root->a_.emplace_back(A_00::ptr{});
      auto b3 = root->a_.emplace_back(domain.CreateObj(A_00::kClassId, 3));
      b3->i_ = 3;
      b3.SetFlags(aether::ObjFlags::kUnloadedByDefault);
      auto& b4 = root->a_.emplace_back(domain.CreateObj(A_00::kClassId, 4));
      b4->i_ = 4;
      erased.clear();
      b4.Serialize();
      b4.Unload();
      REQUIRE((erased == std::set{4}));
      erased.clear();
      root.Serialize();
      int sdf=0;
    }
    REQUIRE((erased == std::set{666, 1, 3}));
    erased.clear();
    {
      aether::Domain domain{nullptr};
      domain.load_facility_ = loader;
      domain.enumerate_facility_ = enumerator;
      A_00::ptr root;
      root.SetId(666);
      root.Load(&domain);
      REQUIRE(!!root);
      REQUIRE(root->i_ == 666);
      root->a_[2].Load(&domain);
      REQUIRE(!!root->a_[2]);
      REQUIRE(root->a_[2]->i_ == 3);

      REQUIRE(!root->a_[3]);
      root->a_[3].Load(&domain);
      REQUIRE(!!root->a_[3]);
      REQUIRE(root->a_[3]->i_ == 4);
    }
    REQUIRE((erased == std::set{666, 1, 3, 4}));
  }
}

// Loads an object with the first reference only and links all other references to the existing object.
void LoadReference() {
  {
    aether::Domain domain{nullptr};
    domain.store_facility_ = saver;
    A_00::ptr root(domain.CreateObj(A_00::kClassId, 666));
    root->i_ = 666;
    auto a = root->a_.emplace_back(domain.CreateObj(A_00::kClassId, 1));
    a->i_ = 1;
    root->a_.emplace_back(root->a_.back());
    serializator_count = 0;
    erased.clear();
    std::filesystem::remove_all("state");
    root.Serialize();
    REQUIRE(serializator_count == 2);
  }
  {
    aether::Domain domain{nullptr};
    domain.load_facility_ = loader;
    domain.enumerate_facility_ = enumerator;
    A_00::ptr root;
    root.SetId(666);
    serializator_count = 0;
    root.Load(&domain);
    REQUIRE(!!root);
    REQUIRE(root->i_ == 666);
    REQUIRE(serializator_count == 2);
  }
}

void Subdomain() {
  {
    std::filesystem::remove_all("state");
    aether::Domain domain{nullptr};
    domain.store_facility_ = saver;
    aether::Domain domain1{&domain};
    domain1.store_facility_ = saver;

    // Root contains 'a' permanent object and
    // 'b' - loadable object that contains reference to 'a'
    A_00::ptr root(domain.CreateObj(A_00::kClassId, 666));
    root->i_ = 666;
    root->a_.emplace_back(domain.CreateObj(A_00::kClassId, 1))->i_ = 1;
    root->a_.emplace_back(domain1.CreateObj(A_00::kClassId, 2))->i_ = 2;
    root->a_[1]->a_.push_back(root->a_[0]);
    root->a_[1].SetFlags(aether::ObjFlags::kUnloadedByDefault);
    serializator_count = 0;
    root->a_[1].Serialize();
    REQUIRE(serializator_count == 2);
    root->a_[1].Unload();
    serializator_count = 0;
    root.Serialize();
    REQUIRE(serializator_count == 2);
  }
  {
    aether::Domain domain{nullptr};
    domain.load_facility_ = loader;
    domain.enumerate_facility_ = enumerator;
    aether::Domain domain1{&domain};
    domain1.load_facility_ = loader;
    domain1.enumerate_facility_ = enumerator;

    A_00::ptr root;
    root.SetId(666);
    serializator_count = 0;
    root.Load(&domain);
    REQUIRE(serializator_count == 2);
    REQUIRE(!!root);
    REQUIRE(root->i_ == 666);
    REQUIRE(!root->a_[1]);
    REQUIRE(!!root->a_[0]);
    REQUIRE(root->a_[0]->i_ == 1);

    serializator_count = 0;
    root->a_[1].Load(&domain1);
    // a_[1] is loaded but a_[1]->a_[0] is linked to root->a_[0]
    REQUIRE(serializator_count == 1);
    REQUIRE(!!root->a_[1]);
    REQUIRE(root->a_[1]->i_ == 2);
    REQUIRE(!!root->a_[1]->a_[0]);
    REQUIRE(root->a_[1]->a_[0]->i_ == root->a_[0]);
  }
}

void SameObjDiffDomains() {
  {
    aether::Domain domain{nullptr};
    domain.store_facility_ = saver;
    A_00::ptr root(domain.CreateObj(A_00::kClassId, 666));
    root->i_ = 666;
    std::filesystem::remove_all("state");
    root.Serialize();
  }
  {
    aether::Domain domain1{nullptr};
    domain1.load_facility_ = loader;
    domain1.enumerate_facility_ = enumerator;
    aether::Domain domain2{nullptr};
    domain2.load_facility_ = loader;
    domain2.enumerate_facility_ = enumerator;

    A_00::ptr root1;
    root1.SetId(666);
    root1.Load(&domain1);
    REQUIRE(!!root1);
    REQUIRE(root1->i_ == 666);

    A_00::ptr root2;
    root2.SetId(666);
    root2.Load(&domain2);
    REQUIRE(!!root2);
    REQUIRE(root2->i_ == 666);

    REQUIRE(root1.ptr_ != root2.ptr_);
  }
}

void Versioning() {
  std::filesystem::remove_all("state");
  Versioning1();
  Pointers();
  SubgraphReleasing();
  Serialization();
  LoadReference();
  Subdomain();
  SameObjDiffDomains();
}




/* =====================================================================================================================
 Multiple interfaces and implementation inheritance example:
class Obj1 {
public:
  virtual ~Obj1() {}
};

class Texture : public Obj1 {
public:
  virtual int* GetHandle() = 0;
  virtual int w() const = 0;
  virtual int h() const = 0;
};

class TextureRead : public virtual Texture {
public:
  virtual int GetPixel(int x, int y) const = 0;
};

class TextureWrite : public virtual Texture {
public:
  virtual void SetPixel(int x, int y, int p) = 0;
};

class TextureImpl : public virtual Texture {
public:
  TextureImpl(int _width, int _height) : width(_width), height(_height) {
    data.resize(width * height, 0);
  }
  virtual int w() const { return width; }
  virtual int h() const { return height; }
  virtual int* GetHandle() { return data.data(); }
protected:
  int width, height;
  std::vector<int> data;
};

class TextureReadImpl : public TextureImpl, public TextureRead {
public:
  TextureReadImpl(int _width, int _height) : TextureImpl(_width, _height) { }
  virtual int GetPixel(int x, int y) const { return data[y * w() + x]; }
};

class TextureWriteImpl : public TextureImpl, public TextureWrite {
public:
  TextureWriteImpl(int _width, int _height) : TextureImpl(_width, _height) { }
  virtual void SetPixel(int x, int y, int p) { data[y * w() + x] = p;  }
};

// class TextureRWImpl : public TextureRead, public TextureWriteImpl {
class TextureRWImpl : public TextureReadImpl, public TextureWrite {
public:
  //  TextureRWDirect(int _width, int _height) : TextureWriteImpl(_width, _height) {}
  //  virtual int GetPixel(int x, int y) const { return data[y * w() + x]; }
  TextureRWImpl(int _width, int _height) : TextureReadImpl(_width, _height) {}
  virtual void SetPixel(int x, int y, int p) { data[y * w() + x] = p;  }
};

void Versioning() {
  auto t0 = new TextureRWImpl(1,1);
  t0->SetPixel(0,0,123);
  auto r1 = t0->GetHandle();
  auto r2 = t0->GetPixel(0,0);
  auto r3 = t0->w();
  auto r4 = t0->h();
  auto t1 = static_cast<Texture*>(t0);
  auto r5 = t1->w();
  auto t2 = static_cast<TextureRead*>(t0);
  auto r6 = t2->w();
  auto t3 = static_cast<Texture*>(t2);
  auto r7 = t3->w();
  int sdf=0;
};
===================================================================================================================== */
