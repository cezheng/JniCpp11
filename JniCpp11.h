#pragma once

#include <exception>
#include <memory>
#include <sstream>
#include <string>

#include <jni.h>

#define CONCAT(A, B, C) A##B##C
#define JNI_FUNC(JAVA_CLASS, METHOD) JNIEXPORT void JNICALL CONCAT(JAVA_CLASS, _, METHOD)

namespace jnicpp11 {

typedef std::shared_ptr<_jobject> shared_jobject;
typedef std::shared_ptr<_jclass> shared_jclass;

class Jni {
 public:
  static JNIEnv *getEnv();
  static JavaVM *getJvm();
  /**
   *  Please make sure to call setJvm if you are not using cocos2d-x
   *
   *  jint JNI_OnLoad(JavaVM *vm, void *reserved) {
   *    Jni::setJvm(vm);
   *   return JNI_VERSION_1_4;
   *  }
   */
  static void setJvm(JavaVM *jvm);

 private:
  static Jni &get();
  JavaVM *_jvm = nullptr;
};

class JniException : public std::exception {
 public:
  static void checkException(JNIEnv *env) throw(JniException);

  JniException(const std::string &message);
  ~JniException() noexcept override {}
  const char *what() const noexcept override;
  void log() const noexcept;

 private:
  std::string _message;
};

class JavaObject;

class JavaClass {
 public:
  static JavaClass getClass(const std::string &classPath);
  virtual ~JavaClass() = default;
  jclass getJClass() const;
  std::string getTypeSignature() const;
  std::string getClassPath() const;

  template <typename... Args> JavaObject newObject(Args... args) const;

  template <typename ReturnType, typename... Args>
  ReturnType staticCall(const std::string &methodName, const ReturnType &defaultValue, Args... args) const;

  template <typename... Args> void staticCallVoid(const std::string &methodName, Args... args) const;

  template <typename ReturnType> ReturnType staticField(const std::string &fieldName, const ReturnType &defaultValue) const;

  operator bool() const;

  bool operator==(const std::nullptr_t &null) const;

 private:
  friend class JavaObject;

  JNIEnv *checkAndGetEnv() const throw(JniException);
  JavaClass(jclass clazz);
  JavaClass(const std::string &classPath);
  JavaClass(jclass clazz, const std::string &classPath);

  JavaObject _newObject(JNIEnv *env, jmethodID methodId, ...) const;

  template <typename ReturnType, typename... Args> ReturnType _staticCall(JNIEnv *env, jmethodID methodId, Args... args) const;

  template <typename ReturnType> ReturnType __staticCall(JNIEnv *env, jmethodID methodId, ...) const;

  template <typename ReturnType> ReturnType _staticField(JNIEnv *env, jfieldID fieldId) const;

  std::string _classPath;
  shared_jclass _jclazz;
};

class JavaObject {
 public:
  JavaObject(jobject obj);
  JavaObject(jobject obj, jclass clazz);
  JavaObject(jobject obj, const JavaClass &clazz);
  JavaObject(jobject obj, const std::string &classPath);

  static JavaObject null(const std::string &classPath);

  virtual ~JavaObject() = default;

  jclass getJClass() const;
  jobject getJObject() const;

  JavaObject asType(const JavaClass &clazz) const;

  virtual std::string getClassPath() const;
  virtual std::string getTypeSignature() const;

  template <typename ReturnType> ReturnType field(const std::string &fieldName, const ReturnType &defaultValue) const;

  template <typename ReturnType, typename... Args>
  ReturnType call(const std::string &methodName, const ReturnType &defaultValue, Args... args) const;

  template <typename... Args> void callVoid(const std::string &methodName, Args... args) const;

  operator bool() const;

  bool operator==(const std::nullptr_t &null) const;

 protected:
  JNIEnv *checkAndGetEnv() const throw(JniException);

  template <typename ReturnType> ReturnType _field(JNIEnv *env, jfieldID fieldId) const;

  template <typename ReturnType, typename... Args> ReturnType _call(JNIEnv *env, jmethodID methodId, Args... args) const;

  template <typename ReturnType> ReturnType __call(JNIEnv *env, jmethodID methodId, ...) const;

  shared_jobject _jobject;
  JavaClass _javaClass;
};

template <typename T> class JavaArray : public JavaObject {
 public:
  std::string getTypeSignature() const override;
};

template <> class JavaArray<JavaObject> : public JavaObject {
 public:
  JavaArray(jobject obj);
  JavaArray(jobject obj, const std::string &elementClassPath);
  static JavaArray<JavaObject> null(const std::string &elementClassPath);
  std::string getTypeSignature() const override;
  std::string getElementClassPath() const;

 private:
  std::string _elementClassPath;
};

#pragma mark - jstring cast methods

std::string fromJString(jstring jstr, const std::string &defaultValue = "", bool deleteLocalRef = false);
std::string fromJString(const JavaObject &jstr, const std::string &defaultValue = "");
JavaObject toJString(const std::string &str);

#pragma mark - makeArg, adaptArg

JavaObject makeArg(const std::string &str);
template <typename T> const T &makeArg(const T &arg) { return arg; }
template <typename T> const T &adaptArg(const T &arg) { return arg; }
jobject adaptArg(const JavaObject &javaObject);

#pragma mark - TypeSignature, MethodSignature

struct TypeSignature {
  static inline std::string get(const JavaObject &jobj) { return jobj.getTypeSignature(); }

  template <typename T> static inline std::string get(const JavaArray<T> &jarr) { return jarr.getTypeSignature(); }

  template <typename T = void> static std::string get();
  template <typename T> static std::string get(const T &) { return get<T>(); }
};

struct MethodSignature {
  template <typename ReturnType, typename... Ts> static std::string get(const ReturnType &ret, Ts... args) {
    return getSigned(TypeSignature::get(ret), std::forward<Ts>(args)...);
  }

  template <typename... Ts> static std::string getVoid(Ts... args) {
    return getSigned(TypeSignature::get(), std::forward<Ts>(args)...);
  }

  template <typename... Ts> static std::string getSigned(const std::string &returnTypeSignature, Ts... args) {
    std::ostringstream os;
    os << "(";
    build(os, std::forward<Ts>(args)...);
    os << ")" << returnTypeSignature;
    return os.str();
  }

  template <typename T, typename... Ts> static void build(std::ostringstream &os, T first, Ts... args) {
    os << TypeSignature::get(std::forward<T>(first));
    MethodSignature::build(os, std::forward<Ts>(args)...);
  }

  static void build(std::ostringstream &os) {}
};

#pragma mark - JniEnv utils
namespace env_util {
jclass findClass(JNIEnv *env, const std::string &classPath) throw(JniException);

jmethodID getMethodId(
    JNIEnv *env, jclass clazz, const std::string &methodName, const std::string &signature, bool isStatic) throw(JniException);

jfieldID getFieldId(JNIEnv *env, jclass clazz, const std::string &fieldName, const std::string &signature, bool isStatic) throw(
    JniException);
}

#pragma mark - JavaClass template methods

template <typename... Args> JavaObject JavaClass::newObject(Args... args) const {
  JNIEnv *env = nullptr;
  try {
    constexpr const char *name = "<init>";
    env = checkAndGetEnv();
    std::string signature = MethodSignature::getVoid(args...);
    jmethodID methodId = env_util::getMethodId(env, getJClass(), name, signature, false);
    JavaObject jinstance = _newObject(env, methodId, makeArg(args)...);
    JniException::checkException(env);
    return jinstance;
  } catch (const JniException &e) {
    e.log();
  }
  return nullptr;
}

template <typename ReturnType>
ReturnType JavaClass::staticField(const std::string &fieldName, const ReturnType &defaultValue) const {
  JNIEnv *env = nullptr;
  try {
    env = checkAndGetEnv();
    std::string signature = TypeSignature::get(defaultValue);
    jfieldID fieldId = env_util::getFieldId(env, getJClass(), fieldName, signature, true);
    auto result = _staticField<ReturnType>(env, fieldId);
    JniException::checkException(env);
    return result;
  } catch (const JniException &e) {
    e.log();
  }
  return defaultValue;
}

template <typename ReturnType, typename... Args>
ReturnType JavaClass::staticCall(const std::string &methodName, const ReturnType &defaultValue, Args... args) const {
  JNIEnv *env = nullptr;
  try {
    env = checkAndGetEnv();
    std::string signature = MethodSignature::get(defaultValue, args...);
    jmethodID methodId = env_util::getMethodId(env, getJClass(), methodName, signature, true);
    auto result = _staticCall<ReturnType>(env, methodId, makeArg(args)...);
    JniException::checkException(env);
    return result;
  } catch (const JniException &e) {
    e.log();
  }
  return defaultValue;
}

template <typename... Args> void JavaClass::staticCallVoid(const std::string &methodName, Args... args) const {
  JNIEnv *env = nullptr;
  try {
    env = checkAndGetEnv();
    std::string signature = MethodSignature::getVoid(args...);
    jmethodID methodId = env_util::getMethodId(env, getJClass(), methodName, signature, true);
    _staticCall<void>(env, methodId, makeArg(args)...);
    JniException::checkException(env);
  } catch (const JniException &e) {
    e.log();
  }
}

template <typename ReturnType, typename... Args>
ReturnType JavaClass::_staticCall(JNIEnv *env, jmethodID methodId, Args... args) const {
  return __staticCall<ReturnType>(env, methodId, adaptArg(args)...);
}

#pragma mark - JavaObject template methods

template <typename ReturnType> ReturnType JavaObject::field(const std::string &fieldName, const ReturnType &defaultValue) const {
  JNIEnv *env = nullptr;
  try {
    env = checkAndGetEnv();
    std::string signature = TypeSignature::get(defaultValue);
    jfieldID fieldId = env_util::getFieldId(env, getJClass(), fieldName, signature, false);
    auto result = _field<ReturnType>(env, fieldId);
    JniException::checkException(env);
    return result;
  } catch (const JniException &e) {
    e.log();
  }
  return defaultValue;
}

template <typename ReturnType, typename... Args>
ReturnType JavaObject::call(const std::string &methodName, const ReturnType &defaultValue, Args... args) const {
  JNIEnv *env = nullptr;
  try {
    env = checkAndGetEnv();
    std::string signature = MethodSignature::get(defaultValue, args...);
    jmethodID methodId = env_util::getMethodId(env, getJClass(), methodName, signature, false);
    auto result = _call<ReturnType>(env, methodId, makeArg(args)...);
    JniException::checkException(env);
    return result;
  } catch (const JniException &e) {
    e.log();
  }
  return defaultValue;
}

template <typename... Args> void JavaObject::callVoid(const std::string &methodName, Args... args) const {
  JNIEnv *env = nullptr;
  try {
    env = checkAndGetEnv();
    std::string signature = MethodSignature::getVoid(args...);
    jmethodID methodId = env_util::getMethodId(env, getJClass(), methodName, signature, false);
    _call<void>(env, methodId, makeArg(args)...);
    JniException::checkException(env);
  } catch (const JniException &e) {
    e.log();
  }
}

template <typename ReturnType, typename... Args>
ReturnType JavaObject::_call(JNIEnv *env, jmethodID methodId, Args... args) const {
  return __call<ReturnType>(env, methodId, adaptArg(args)...);
}

#pragma mark - JavaArray
template <typename T> std::string JavaArray<T>::getTypeSignature() const { return "[" + TypeSignature::get<T>(); }

}  // namespace jnicpp11