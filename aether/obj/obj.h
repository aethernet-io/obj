/* Copyright 2016 Aether authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef AETHER_OBJ_H_
#define AETHER_OBJ_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <cassert>
#include "../../third_party/crc32/crc32.h"

namespace aether {
class Domain;
class Obj;
}

//#ifndef AETHER_OMSTREAM
#include "../../third_party/aether/stream/aether/mstream/mstream.h"
#define AETHER_OMSTREAM aether::omstream<aether::Domain*>
#define AETHER_IMSTREAM aether::imstream<aether::Domain*>
//#endif

namespace aether {

class ObjId {
public:
  using Type = uint32_t;
  ObjId() = default;
  ObjId(const Type& i) : id_(i) {}
  static ObjId GenerateUnique() {
    static int i=0;
    return ++i;//std::rand());
  }
  void Invalidate() { id_ = 0; }
  bool IsValid() const { return id_ != 0; }
  bool operator < (const ObjId& i) const { return id_ < i.id_; }
  friend AETHER_OMSTREAM& operator << (AETHER_OMSTREAM& s, const ObjId& i) { return s << i.id_; }
  friend AETHER_IMSTREAM& operator >> (AETHER_IMSTREAM& s, ObjId& i) { return s >> i.id_; }
  
  std::string ToString() const { return std::to_string(id_); }
protected:
  Type id_;
};

class ObjFlags {
  uint8_t value_;
public:
  enum { kLoadable = 1 << 0, kLoaded = 1 << 1, kConst = 1 << 2, };
  operator uint8_t&() { return value_; }
  ObjFlags(decltype(value_) v) : value_(v) {}
  ObjFlags() : value_(kLoaded) {}
};

using ObjStorage = uint8_t;
using StoreFacility = std::function<void(const std::string& path, ObjStorage storage, const AETHER_OMSTREAM& os)>;
using LoadFacility = std::function<void(const std::string& path, ObjStorage storage, AETHER_IMSTREAM& is)>;

template <class T> class Ptr {
 public:
  // If pointer is null then Id, flags and storage are in the pointer.
  T* ptr_;
  ObjId id_;
  ObjFlags flags_;
  ObjStorage storage_;
  
  // Example: A::ptr a;
  Ptr() { ptr_ = nullptr; }
  // Example: A::ptr a(nullptr);
  // Example: a = nullptr;
  Ptr(T* p) { Init(p); }
  // Example: A::ptr a(other_a_ptr);
  // Example: A::ptr a = other_a_ptr;
  Ptr(const Ptr& p) { Init(p.ptr_); }
  // Example: B::ptr b((A*)nullptr);
  template <class T1> Ptr(T1* p) { InitCast(p); }
  // Example: B::ptr b(a);
  // Example: B::ptr b = a;
  template <class T1> Ptr(const Ptr<T1>& p) { InitCast(p.ptr_); }
  // Example: A::ptr a2(std::move(a1));
  Ptr(Ptr&& p) {
    ptr_ = p.ptr_;
    p.ptr_ = nullptr;
  }
  // Example: B::ptr b(std::move(a));
  template <class T1> Ptr(Ptr<T1>&& p) {
    if (!p) {
      ptr_ = nullptr;
      return;
    }
    T* ptr = static_cast<T*>(p.ptr_->DynamicCast(T::class_id_));
    if (ptr) {
      ptr_ = ptr;
      p.ptr_ = nullptr;
    } else {
      ptr_ = nullptr;
      // Can't cast to this pointer so just release the original pointer.
      p.Release();
    }
  }

  // Example: a1 = a2;
  Ptr& operator = (const Ptr& p) {
    // The object is the same. It's ok to compare pointers because the class is
    // also the same. Copy to itself also detected here.
    if (ptr_ != p.ptr_) {
      // Release the old pointer first.
      Release();
      Init(p.ptr_);
    }
    return *this;
  }
  // Example: b = a;
  template <class T1> Ptr& operator = (const Ptr<T1>& p) {
    // Same object copy.
    if (*this == p) {
      return *this;
    }
    Release();
    InitCast(p.ptr_);
    return *this;
  }
  // Example: a2 = std::move(a1);
  Ptr& operator = (Ptr&& p) {
    // Don't move to itself.
    if (this != &p) {
      if (ptr_ == p.ptr_) {
        // Pointing to the same object - release the source.
        p.Release();
      } else {
        // Another object is comming.
        Release();
        ptr_ = p.ptr_;
        p.ptr_ = nullptr;
      }
    }
    return *this;
  }
  // Example: b = std::move(a);
  template <class T1> Ptr& operator = (Ptr<T1>&& p) {
    if (!p) {
      Release();
    } else {
      T* ptr = static_cast<T*>(p.ptr_->DynamicCast(T::class_id_));
      if (!ptr) {
        Release();
      } else {
        ptr_ = ptr;
        p.ptr_ = nullptr;
      }
    }
    return *this;
  }

  ~Ptr() { Release(); }
  
  operator bool() const { return ptr_; }
  T* operator->() const { return ptr_; }
  
  const ObjId& GetId() const { return ptr_ ? ptr_->id_ : id_; }
  void SetId(const ObjId& i) {
    if (ptr_) ptr_->id_ = i;
    else id_ = i;
  }
  ObjFlags GetFlags() const { return ptr_ ? ptr_->flags_ : flags_; }
  void SetFlags(ObjFlags flags) {
    if (ptr_) ptr_->flags_ = flags;
    else flags_ = flags;
  }
  ObjStorage GetStorage() const { return ptr_ ? ptr_->storage_ : storage_; }
  void SetStorage(ObjStorage storage) {
    if (ptr_) ptr_->storage_ = storage;
    else storage_ = storage;
  }

  void Serialize(StoreFacility s, int flags) const;
  void Unload();
  void Load(LoadFacility l);
  Ptr Clone(LoadFacility load_facility) const;

  // Protected section.
  void Init(T* p);  
  template <class T1> void InitCast(T1* p) { Init(p ? static_cast<T*>(p->DynamicCast(T::class_id_)) : nullptr); }
  void Release();
  static constexpr uint32_t kObjClassId = qcstudio::crc32::from_literal("Obj").value;
};

// Same type comparison.
template <class T1> bool operator == (const Ptr<T1>& p1, const Ptr<T1>& p2) { return p1.ptr_ == p2.ptr_; }
template <class T1> bool operator != (const Ptr<T1>& p1, const Ptr<T1>& p2) { return !(p1 == p2); }
// Different type comparison.
template <class T1, class T2> bool operator == (const Ptr<T1>& p1, const Ptr<T2>& p2) {
  // If one or both pointers are zero - not equal.
  if (!p1 || !p2) return false;
  
  constexpr uint32_t class_id = qcstudio::crc32::from_literal("Obj").value;
  return p1.ptr_->DynamicCast(class_id) == p2.ptr_->DynamicCast(class_id);
}
template <class T1, class T2> bool operator != (const Ptr<T1>& p1, const Ptr<T2>& p2) { return !(p1 == p2); }


template <class T, class T1> void SerializeObj(T& s, const Ptr<T1>& o1);
template <class T> Ptr<Obj> DeserializeObj(T& s);

#define AETHER_SERIALIZE_(CLS, BASE) \
  virtual void Serialize(AETHER_OMSTREAM& s) { Serializator(s, s.custom_->flags_); } \
  virtual void Deserialize(AETHER_IMSTREAM& s) { Serializator(s, s.custom_->flags_); } \
  friend AETHER_OMSTREAM& operator << (AETHER_OMSTREAM& s, const CLS::ptr& o) { \
    SerializeObj(s, o); \
    return s; \
  } \
  friend AETHER_IMSTREAM& operator >> (AETHER_IMSTREAM& s, CLS::ptr& o) { \
    o = DeserializeObj(s); \
    return s; \
  }

#define AETHER_SERIALIZE_CLS1(CLS) AETHER_SERIALIZE_(CLS, Obj)
#define AETHER_SERIALIZE_CLS2(CLS, CLS1) AETHER_SERIALIZE_(CLS, CLS1)
#define AETHER_GET_MACRO(_1, _2, NAME, ...) NAME
#define AETHER_SERIALIZE(...) AETHER_VSPP(AETHER_GET_MACRO(__VA_ARGS__, \
AETHER_SERIALIZE_CLS2, AETHER_SERIALIZE_CLS1)(__VA_ARGS__))
// VS++ bug
#define AETHER_VSPP(x) x


#define AETHER_INTERFACES(...) \
  template <class ...> struct ClassList {}; void* DynamicCastInternal(uint32_t, ClassList<>) { return nullptr; }\
  template <class C, class ...N> void* DynamicCastInternal(uint32_t i, ClassList<C, N...>) {\
    if (C::class_id_ != i) return DynamicCastInternal(i, ClassList<N...>()); \
    return static_cast<C*>(this); \
  } \
  template <class ...N> void* DynamicCastInternal(uint32_t i) { return DynamicCastInternal(i, ClassList<N...>()); }\
  virtual void* DynamicCast(uint32_t id) { return DynamicCastInternal<__VA_ARGS__, Obj>(id); }

#define AETHER_OBJECT(CLS) \
  typedef aether::Ptr<CLS> ptr; \
  static aether::Obj::Registrar<CLS> registrar_; \
  static constexpr uint32_t class_id_ = qcstudio::crc32::from_literal(#CLS).value; \
  virtual uint32_t GetClassId() const { return class_id_; }

#define AETHER_PURE_INTERFACE(CLS) \
  AETHER_OBJECT(CLS) \
  AETHER_INTERFACES(CLS)

#define AETHER_IMPLEMENTATION(CLS) aether::Obj::Registrar<CLS> CLS::registrar_(CLS::class_id_, 0);

class Domain {
public:
  int flags_;
  StoreFacility store_facility_;
  LoadFacility load_facility_;
  std::unordered_map<Obj*, int> objects_;
  bool FindAndAddObject(Obj* o) {
    auto& references_count = objects_[o];
    references_count++;
    return references_count > 1;
  }
};

class Obj {
protected:
  template <class T> struct Registrar {
    Registrar(uint32_t cls_id, uint32_t base_id) {
      Obj::Registry<void>::RegisterClass(cls_id, base_id, []{ return new T(); });
    }
  };

public:
  static Obj* CreateClassById(uint32_t cls_id, ObjId obj_id) {
    Obj* o = Registry<void>::CreateClassById(cls_id);
    o->id_ = obj_id;
    return o;
  }

  Obj() {
    id_ = ObjId::GenerateUnique();
    flags_ = ObjFlags::kLoaded;
    storage_ = 0;
  }
  virtual ~Obj() {
    auto it = Registry<void>::all_objects_.find(id_);
    if (it != Registry<void>::all_objects_.end()) Registry<void>::all_objects_.erase(it);
  }
  
  virtual void OnLoaded() {}
  
  static void AddObject(Obj* o) {
    Registry<void>::all_objects_[o->id_] = o;
  }
  
  static void RemoveObject(Obj* o) {
    auto it = Registry<void>::all_objects_.find(o->id_);
    assert(it != Registry<void>::all_objects_.end());
    Registry<void>::all_objects_.erase(it);
  }
  
  static Obj* FindObject(ObjId obj_id) {
    auto it = Registry<void>::all_objects_.find(obj_id);
    if (it != Registry<void>::all_objects_.end()) return it->second;
    return nullptr;
  }

  AETHER_OBJECT(Obj);
  AETHER_INTERFACES(Obj);
  AETHER_SERIALIZE(Obj);
  template <typename T> void Serializator(T& s, int flags) const {}

  ObjId id_;
  ObjFlags flags_;
  ObjStorage storage_;
  enum Serialization { kConsts = 1 << 0, kData = 1 << 1, kRefs = 1 << 2 };
protected:
  template <class Dummy> class Registry {
  public:
    static void RegisterClass(uint32_t cls_id, uint32_t base_id, std::function<Obj*()> factory) {
      static bool initialized = false;
      if (!initialized) {
        initialized = true;
        registry_ = new std::unordered_map<uint32_t, std::function<Obj*()>>();
        base_to_derived_ = new std::unordered_map<uint32_t, uint32_t>();
      }
      if (registry_->find(cls_id) != registry_->end()) {
        throw std::runtime_error("Class name already registered or Crc32 collision detected. Please choose another "
                                 "name for the class.");
      }
      (*registry_)[cls_id] = factory;
      if (base_id != qcstudio::crc32::from_literal("Obj").value) (*base_to_derived_)[base_id] = cls_id;
    }
    
    static void UnregisterClass(uint32_t cls_id) {
      auto it = registry_->find(cls_id);
      if (it != registry_->end()) registry_->erase(it);
      for (auto it = base_to_derived_->begin(); it != base_to_derived_->end(); ) {
        it = it->second == cls_id ? base_to_derived_->erase(it) : std::next(it);
      }
    }
    
    static Obj* CreateClassById(uint32_t base_id) {
      uint32_t derived_id = base_id;
      while (true) {
        auto d = base_to_derived_->find(derived_id);
        if (d == base_to_derived_->end() || derived_id == d->second) break;
        derived_id = d->second;
      }
      auto it = registry_->find(derived_id);
      if (it == registry_->end()) return nullptr;
      return it->second();
    }
    
    static std::map<ObjId, Obj*> all_objects_;
    static bool first_release_;
  private:
    static std::unordered_map<uint32_t, std::function<Obj*()>>* registry_;
    static std::unordered_map<uint32_t, uint32_t>* base_to_derived_;
  };
  template <class T> friend class Ptr;
  friend class Domain;
  int reference_count_ = 0;
  friend class TestAccessor;
};

template <class Dummy> std::unordered_map<uint32_t, std::function<Obj*()>>* Obj::Registry<Dummy>::registry_;
template <class Dummy> std::unordered_map<uint32_t, uint32_t>* Obj::Registry<Dummy>::base_to_derived_;
template <class Dummy> bool Obj::Registry<Dummy>::first_release_ = true;
template <class Dummy> std::map<ObjId, Obj*> Obj::Registry<Dummy>::all_objects_;

template <class T, class T1> void SerializeObj(T& s, const Ptr<T1>& o) {
  if (!o && !(o.GetFlags() & ObjFlags::kLoadable)) {
    s << ObjId{0} << ObjFlags{} << ObjStorage{0};
    return;
  }
  s << o.GetId() << o.GetFlags() << o.GetStorage();
  if (!o || s.custom_->FindAndAddObject(o.ptr_)) return;
  
  // Don't serialize constant objects if not directed.
  if (!(s.custom_->flags_ & Obj::Serialization::kConsts) && (o.GetFlags() & ObjFlags::kConst)) return;

  AETHER_OMSTREAM os;
  os.custom_ = s.custom_;
  os << o->GetClassId();
  o->Serialize(os);
  s.custom_->store_facility_(o.GetId().ToString(), o.GetStorage(), os);
}

template <typename T> void Ptr<T>::Release() {
  if (ptr_) {
    if (--ptr_->reference_count_ == 0) delete ptr_;
    ptr_ = nullptr;
  }

//  if (Obj::Registry<void>::first_release_) {
//    Obj::Registry<void>::first_release_ = false;
//
//    // Count all references to all objects which are accessible from this pointer that is going to be released.
//    Domain domain;
//    domain.flags_ = Obj::Serialization::kRefs | Obj::Serialization::kConsts;
//    domain.store_facility_ = [](const std::string&, ObjStorage, const AETHER_OMSTREAM&) {};
//    AETHER_OMSTREAM os2;
//    os2.custom_ = &domain;
//    os2 << *this;
//
//    std::vector<Obj*> release;
//    std::vector<Obj*> keep;
//    for (auto it : domain.objects_) {
//      if (it.first->reference_count_ == it.second) release.push_back(it.first);
//      else keep.push_back(it.first);
//    }
//
//    // If a candidate for releasing is referenced directly or indirectly by the object that is kept then don't release.
//    for (auto k : keep) {
//      domain.objects_.clear();
//      k->Serialize(os2);
//      for (auto it = release.begin(); it != release.end(); ) {
//        if (domain.objects_.find(*it) != domain.objects_.end()) it = release.erase(it);
//        else ++it;
//      }
//    }
//
//    // Maually release each object without recursive releasing.
//    for (auto r : release) r->reference_count_ = 0;
//    for (auto r : release) delete r;
//    Obj::Registry<void>::first_release_ = true;
//  }
}

template <class T> Obj::ptr DeserializeObj(T& s) {
  ObjId obj_id;
  ObjFlags obj_flags;
  ObjStorage obj_storage;
  s >> obj_id >> obj_flags >> obj_storage;
  if (!obj_id.IsValid()) return {};
  if(!(obj_flags & ObjFlags::kLoaded)) {
    Obj::ptr o;
    // Distinguish 'unloaded' from 'nullptr'
    o.SetId(obj_id);
    o.SetFlags(obj_flags);
    o.SetFlags(obj_storage);
    return o;
  }

  // If object is already deserialized.
  Obj* obj = Obj::FindObject(obj_id);
  if (obj) return obj;
  
  AETHER_IMSTREAM is;
  is.custom_ = s.custom_;
  s.custom_->load_facility_(obj_id.ToString(), obj_storage, is);
  uint32_t class_id;
  is >> class_id;
  obj = Obj::CreateClassById(class_id, obj_id);
  obj->id_ = obj_id;
  obj->flags_ = obj_flags;
  obj->storage_ = obj_storage;
  // Add object to the list of already loaded before deserialization to avoid infinite loop of cyclic references.
  Obj::AddObject(obj);
  obj->Deserialize(is);
  // Track all deserialized objects.
  is.custom_->FindAndAddObject(obj);
  return obj;
}


template <typename T> void Ptr<T>::Serialize(StoreFacility store_facility, int flags) const {
//  Domain domain;
//  domain.flags_ = flags;
//  domain.store_facility_ = store_facility;
//  AETHER_OMSTREAM os;
//  os.custom_ = &domain;
//  os << *this;
}

template <typename T> void Ptr<T>::Unload() {
//  auto o = NewPlaceholder();
//  id_ = GetId();
//  flags_ = GetFlags() & (~ObjFlags::kLoaded);
//  o->storage_ = GetStorage();
//  Release();
//  ptr_ = o;
}

template <typename T> void Ptr<T>::Load(LoadFacility load_facility) {
//  if (!IsPlaceholder()) return;
//  AETHER_IMSTREAM is;
//  Domain domain;
//  domain.flags_ = Obj::Serialization::kRefs | Obj::Serialization::kData | Obj::Serialization::kConsts;
//  domain.load_facility_ = load_facility;
//  is.custom_ = &domain;
//  AETHER_OMSTREAM os;
//  os << GetId() << (GetFlags() | ObjFlags::kLoaded) << GetStorage();
//  is.stream_ = std::move(os.stream_);
//  Obj::Registry<void>::first_release_ = false;
//  is >> *this;
//  Obj::Registry<void>::first_release_ = true;
//  for (auto it : domain.objects_) {
//    it.first->OnLoaded();
//  }
}

template <typename T> void Ptr<T>::Init(T* p) {
  ptr_ = p;
  if (ptr_) ptr_->reference_count_++;
}

template <typename T> Ptr<T> Ptr<T>::Clone(LoadFacility load_facility) const {
//  // Clone whole hierarchy from the unloaded subgraph.
//  if ((GetFlags() & ObjFlags::kLoadable) && !(GetFlags() & ObjFlags::kLoaded)) {
//    AETHER_OMSTREAM os;
//    os << GetId() << (GetFlags() | ObjFlags::kLoaded) << GetStorage();
//    AETHER_IMSTREAM is;
//    is.stream_ = std::move(os.stream_);
//    Domain domain;
//    domain.flags_ = Obj::Serialization::kRefs | Obj::Serialization::kData | Obj::Serialization::kConsts;
//    domain.load_facility_ = load_facility;
//    is.custom_ = &domain;
//    Obj::ptr o;
//    Obj::Registry<void>::first_release_ = false;
//    is >> o;
//    Obj::Registry<void>::first_release_ = true;
//    // Make Ids of loaded objects unique and re-register objects globally.
//    for (auto it : domain.objects_) {
//      Obj::RemoveObject(it.first);
//      it.first->id_ = ObjId::GenerateUnique();
//      Obj::AddObject(it.first);
//    }
//    return o;
//  }
  return {};
//  std::map<std::string, AETHER_OMSTREAM> data;
//  Domain domain;
//  domain.flags_ = Obj::Serialization::kRefs | Obj::Serialization::kData | Obj::Serialization::kConsts;
//  domain.store_facility_ = [&data](const std::string& path, const AETHER_OMSTREAM& os) {
//    data[path].stream_ = std::move(os.stream_);
//  };
//  domain.load_facility_ = [&data](const std::string& path, AETHER_IMSTREAM& is) {
//    is.stream_ = std::move(data[path].stream_);
//  };
//  AETHER_OMSTREAM os;
//  os.custom_ = &domain;
//  os << *this;
//
//  AETHER_IMSTREAM is;
//  is.stream_ = std::move(os.stream_);
//  is.custom_ = &domain;
//  domain.objects_.clear();
//  Obj::ptr o;
//  is >> o;
//  // Make Ids of loaded objects unique.
//  for (auto it : domain.objects_) it.first->id_ = ObjId::GenerateUnique();
//  return o;
}


}  // namespace aether

#endif  // AETHER_OBJ_H_
