#include "JniCpp11.h"

#include <pthread.h>

#ifdef __ANDROID__

#include <android/log.h>

#define LOG_TAG "JniCpp11"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#else

#define LOGD(...) fprintf(stdout, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)

#endif

#if defined(NDEBUG)
#undef LOGD
#define LOGD(...)
#endif

#ifdef __has_include  // if clang

#if __has_include("jni/JniHelper.h")
#define __USE_COCOS2DX_JVM__ 1
#include "jni/JniHelper.h"
#elif defined(__USE_COCOS2DX_JVM__)
#undef __USE_COCOS2DX_JVM__
#endif

#else  // if gcc

#warning "Compiler does not support __has_include. It is strongly recommended that you use clang rather than gcc."

#define __USE_COCOS2DX_JVM__ 1
#include "jni/JniHelper.h"

#endif

namespace jnicpp11 {

#pragma mark - Jni
static pthread_key_t g_key;
static void detachCurrentThread(void *a) {
  if (Jni::getJvm()) {
    Jni::getJvm()->DetachCurrentThread();
  }
}

Jni &Jni::get() {
  static Jni jni;
  return jni;
}

JNIEnv *Jni::getEnv() {
#ifdef __USE_COCOS2DX_JVM__
  return cocos2d::JniHelper::getEnv();
#else
  JNIEnv *env = (JNIEnv *)pthread_getspecific(g_key);
  if (env == nullptr) {
    JavaVM *jvm = getJvm();
    if (jvm == nullptr) {
      return nullptr;
    }
    jint ret = jvm->GetEnv((void **)&env, JNI_VERSION_1_4);

    switch (ret) {
      case JNI_OK:
        // Success!
        pthread_setspecific(g_key, env);
        return env;

      case JNI_EDETACHED:
        if (jvm->AttachCurrentThread(&env, nullptr) < 0) {
          return nullptr;
        } else {
          pthread_setspecific(g_key, env);
          return env;
        }

      default:
        return nullptr;
    }
  }
  return env;
#endif
}

JavaVM *Jni::getJvm() {
#ifdef __USE_COCOS2DX_JVM__
  return cocos2d::JniHelper::getJavaVM();
#else
  return get()._jvm;
#endif
}

void Jni::setJvm(JavaVM *jvm) {
  get()._jvm = jvm;
  pthread_key_create(&g_key, detachCurrentThread);
}

#pragma mark - static methods
static void globalRefDeleter(jobject jref) {
  if (jref) {
    JNIEnv *env = Jni::getEnv();
    if (env) {
      env->DeleteGlobalRef(jref);
    }
  }
}

static void localRefDeleter(jobject jref) {
  if (jref) {
    JNIEnv *env = Jni::getEnv();
    if (env) {
      env->DeleteLocalRef(jref);
    }
  }
}

template <typename T>
static typename std::enable_if<std::is_base_of<_jobject, T>::value, std::shared_ptr<T>>::type toGlobalRefSharedPtr(T *localRef) {
  if (localRef == nullptr) {
    return nullptr;
  }
  JNIEnv *env = Jni::getEnv();
  if (env == nullptr) {
    return nullptr;
  }
  try {
    T *globalRef = (T *)env->NewGlobalRef(localRef);
    JniException::checkException(env);
    return std::shared_ptr<T>(globalRef, globalRefDeleter);
  } catch (const JniException &e) {
    e.log();
  }
  return nullptr;
}

template <typename T>
static typename std::enable_if<std::is_base_of<_jobject, T>::value, std::shared_ptr<T>>::type toLocalRefSharedPtr(T *localRef) {
  return std::shared_ptr<T>(localRef, localRefDeleter);
}

#pragma mark - JavaClass

JNIEnv *JavaClass::checkAndGetEnv() const throw(JniException) {
  JNIEnv *env = Jni::getEnv();
  if (env == nullptr) {
    throw JniException("Failed to get JNIEnv. ");
  }

  jclass jclazz = getJClass();
  if (jclazz == nullptr) {
    throw JniException("Failed to get jclass.");
  }

  return env;
}

JavaClass JavaClass::getClass(const std::string &classPath) {
  JNIEnv *env = Jni::getEnv();
  if (env == nullptr) {
    return nullptr;
  }
  try {
    jclass clazz = env_util::findClass(env, classPath);
    return JavaClass(clazz, classPath);
  } catch (const JniException &e) {
    e.log();
  }
  return nullptr;
}

JavaClass::JavaClass(jclass clazz) : _jclazz(toGlobalRefSharedPtr(clazz)) {}

JavaClass::JavaClass(const std::string &classPath) : _classPath(classPath) {}

JavaClass::JavaClass(jclass clazz, const std::string &classPath) : _jclazz(toGlobalRefSharedPtr(clazz)), _classPath(classPath) {}

jclass JavaClass::getJClass() const {
  if (_jclazz == nullptr) {
    if (!_classPath.empty()) {
      JNIEnv *env = Jni::getEnv();
      if (env == nullptr) {
        return nullptr;
      }
      try {
        jclass clazz = env_util::findClass(env, _classPath);
        const_cast<JavaClass *>(this)->_jclazz = toGlobalRefSharedPtr(clazz);
        env->DeleteLocalRef(clazz);
      } catch (const JniException &e) {
        e.log();
      }
    }
  }
  return _jclazz.get();
}

std::string JavaClass::getTypeSignature() const { return "L" + getClassPath() + ";"; }

std::string JavaClass::getClassPath() const {
  constexpr const char *defaultRet = "java/lang/Object";

  if (_classPath.empty()) {
    JNIEnv *env = Jni::getEnv();
    if (env == nullptr) {
      return defaultRet;
    }

    jclass clazz = getJClass();
    if (clazz == nullptr) {
      return defaultRet;
    }

    jclass clazzRet = (jclass)env->NewLocalRef(clazz);
    if (clazzRet == nullptr) {
      LOGD("NewLocalRef failed");
      return defaultRet;
    }

    JavaObject classObject(clazz, "java/lang/Class");
    const_cast<JavaClass *>(this)->_classPath = classObject.call<std::string>("getName", "");
    LOGD("java/lang/Class getName result: %s", _classPath.c_str());
  }
  return _classPath;
}

JavaObject JavaClass::_newObject(JNIEnv *env, jmethodID methodId, ...) const {
  va_list args;
  va_start(args, methodId);
  jobject jret = env->NewObjectV(_jclazz.get(), methodId, args);
  va_end(args);
  return JavaObject(jret, *this);
}

JavaClass::operator bool() const { return _jclazz != nullptr; }

bool JavaClass::operator==(const std::nullptr_t &null) const { return _jclazz == null; }

#pragma mark - JavaObject
JavaObject::JavaObject(jobject obj) : _jobject(toLocalRefSharedPtr(obj)), _javaClass(nullptr) {}

JavaObject::JavaObject(jobject obj, jclass clazz) : _jobject(toLocalRefSharedPtr(obj)), _javaClass(clazz) {}

JavaObject::JavaObject(jobject obj, const JavaClass &clazz) : _jobject(toLocalRefSharedPtr(obj)), _javaClass(clazz) {}

JavaObject::JavaObject(jobject obj, const std::string &classPath) : _jobject(toLocalRefSharedPtr(obj)), _javaClass(classPath) {}

JavaObject JavaObject::null(const std::string &classPath) { return JavaObject(nullptr, classPath); }

JNIEnv *JavaObject::checkAndGetEnv() const throw(JniException) {
  JNIEnv *env = Jni::getEnv();
  if (env == nullptr) {
    throw JniException("Failed to get JNIEnv.");
  }

  jclass jclazz = getJClass();
  if (jclazz == nullptr) {
    throw JniException("Failed to get jclass. ");
  }

  jobject jobj = getJObject();
  if (jobj == nullptr) {
    throw JniException("Failed to get jobject. ");
  }
  return env;
}

jclass JavaObject::getJClass() const {
  if (_javaClass == nullptr) {
    if (_jobject) {
      JNIEnv *env = Jni::getEnv();
      if (env == nullptr) {
        return nullptr;
      }
      try {
        jclass clazz = env->GetObjectClass(_jobject.get());
        if (clazz == nullptr) {
          throw JniException("GetObjectClass failed.");
        }
        JniException::checkException(env);
        const_cast<JavaObject *>(this)->_javaClass = JavaClass(clazz);
        env->DeleteLocalRef(clazz);
      } catch (const JniException &e) {
        e.log();
      }
    }
  }
  return _javaClass.getJClass();
}

JavaObject JavaObject::asType(const JavaClass &clazz) const {
  JavaObject ret(nullptr, clazz);
  ret._jobject = _jobject;
  return ret;
}

jobject JavaObject::getJObject() const { return _jobject.get(); }

std::string JavaObject::getClassPath() const {
  (void)getJClass();
  return _javaClass.getClassPath();
}

std::string JavaObject::getTypeSignature() const {
  (void)getJClass();
  return _javaClass.getTypeSignature();
}

JavaObject::operator bool() const { return _jobject != nullptr; }

bool JavaObject::operator==(const std::nullptr_t &null) const { return _jobject == null; }

#pragma mark - JavaArray
JavaArray<JavaObject>::JavaArray(jobject obj) : JavaObject(obj) {}
JavaArray<JavaObject>::JavaArray(jobject obj, const std::string &elementClassPath)
    : JavaObject(obj), _elementClassPath(elementClassPath) {}

JavaArray<JavaObject> JavaArray<JavaObject>::null(const std::string &elementClassPath) {
  return JavaArray<JavaObject>(nullptr, elementClassPath);
}

std::string JavaArray<JavaObject>::getElementClassPath() const {
  // TODO: lazy
  return _elementClassPath;
}

std::string JavaArray<JavaObject>::getTypeSignature() const { return "[L" + getElementClassPath() + ";"; }

#pragma mark - JniException

void JniException::checkException(JNIEnv *env) throw(JniException) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    throw JniException("JNI ExceptionCheck found exception.");
  }
}

JniException::JniException(const std::string &message) : _message(message) {}

const char *JniException::what() const noexcept { return _message.c_str(); }

void JniException::log() const noexcept { LOGE("%s", _message.c_str()); }

#pragma mark - TypeSignatures

template <> std::string TypeSignature::get<std::string>() { return "Ljava/lang/String;"; }

template <> std::string TypeSignature::get<jstring>() { return "Ljava/lang/String;"; }

template <> std::string TypeSignature::get<jobject>() { return "Ljava/lang/Object;"; }

template <> std::string TypeSignature::get<bool>() { return "Z"; }
template <> std::string TypeSignature::get<jboolean>() { return "Z"; }
template <> std::string TypeSignature::get<jbyte>() { return "B"; }
template <> std::string TypeSignature::get<jchar>() { return "C"; }
template <> std::string TypeSignature::get<jshort>() { return "S"; }
template <> std::string TypeSignature::get<jint>() { return "I"; }
template <> std::string TypeSignature::get<unsigned int>() { return "I"; }
template <> std::string TypeSignature::get<jlong>() { return "J"; }
template <> std::string TypeSignature::get<long>() { return "J"; }
template <> std::string TypeSignature::get<jfloat>() { return "F"; }
template <> std::string TypeSignature::get<jdouble>() { return "D"; }
template <> std::string TypeSignature::get<void>() { return "V"; }

template <> std::string TypeSignature::get<jbooleanArray>() { return "[Z"; }
template <> std::string TypeSignature::get<jbyteArray>() { return "[B"; }
template <> std::string TypeSignature::get<jcharArray>() { return "[C"; }
template <> std::string TypeSignature::get<jshortArray>() { return "[S"; }
template <> std::string TypeSignature::get<jintArray>() { return "[I"; }
template <> std::string TypeSignature::get<jlongArray>() { return "[J"; }
template <> std::string TypeSignature::get<jfloatArray>() { return "[F"; }
template <> std::string TypeSignature::get<jdoubleArray>() { return "[D"; }

#pragma mark - Type Casters

JavaObject toJString(const std::string &str) {
  JNIEnv *env = Jni::getEnv();
  jsize strLen = (jsize)str.size();
  if (strLen == 0) {
    return JavaObject(env->NewStringUTF(""));
  }
  JavaClass clazz = JavaClass::getClass("java/lang/String");
  jbyteArray byteArray = nullptr;
  jstring charsetName = nullptr;
  try {
    byteArray = env->NewByteArray(strLen);
    if (byteArray == nullptr) {
      throw JniException("NewByteArray failed.");
    }
    charsetName = env->NewStringUTF("UTF-8");
    if (charsetName == nullptr) {
      throw JniException("NewStringUTF failed.");
    }
    env->SetByteArrayRegion(byteArray, 0, strLen, reinterpret_cast<const jbyte *>(str.data()));
    JniException::checkException(env);
    JavaObject jstr = clazz.newObject(byteArray);
    env->DeleteLocalRef(charsetName);
    env->DeleteLocalRef(byteArray);
    return jstr;
  } catch (const JniException &e) {
    e.log();
    if (byteArray) {
      env->DeleteLocalRef(byteArray);
    }
    if (charsetName) {
      env->DeleteLocalRef(charsetName);
    }
  }
  return nullptr;
}

std::string fromJString(const JavaObject &jstr, const std::string &defaultValue) {
  return fromJString((jstring)jstr.getJObject(), defaultValue, false);
}

std::string fromJString(jstring jstr, const std::string &defaultValue, bool deleteLocalRef) {
  JNIEnv *env = Jni::getEnv();
  const char *chars = env->GetStringUTFChars(jstr, NULL);
  if (chars == nullptr) {
    return defaultValue;
  }
  std::string ret = chars;
  env->ReleaseStringUTFChars(jstr, chars);
  if (deleteLocalRef) {
    env->DeleteLocalRef(jstr);
  }
  return ret;
}

#pragma mark - JavaClass, JavaObject template specializations

#define CALL_STATIC_METHOD(TYPE, TYPE_NAME)                                              \
  template <> TYPE JavaClass::__staticCall(JNIEnv *env, jmethodID methodId, ...) const { \
    va_list args;                                                                        \
    va_start(args, methodId);                                                            \
    auto ret = env->CallStatic##TYPE_NAME##MethodV(_jclazz.get(), methodId, args);       \
    va_end(args);                                                                        \
    return ret;                                                                          \
  }

#define GET_STATIC_FIELD(TYPE, TYPE_NAME)                                         \
  template <> TYPE JavaClass::_staticField(JNIEnv *env, jfieldID fieldId) const { \
    return env->GetStatic##TYPE_NAME##Field(_jclazz.get(), fieldId);              \
  }

#define GET_FIELD(TYPE, TYPE_NAME)                                           \
  template <> TYPE JavaObject::_field(JNIEnv *env, jfieldID fieldId) const { \
    return env->Get##TYPE_NAME##Field(_jobject.get(), fieldId);              \
  }

#define CALL_METHOD(TYPE, TYPE_NAME)                                                \
  template <> TYPE JavaObject::__call(JNIEnv *env, jmethodID methodId, ...) const { \
    va_list args;                                                                   \
    va_start(args, methodId);                                                       \
    auto ret = env->Call##TYPE_NAME##MethodV(_jobject.get(), methodId, args);       \
    va_end(args);                                                                   \
    return ret;                                                                     \
  }

#define TEMPLATE_SPEC(TYPE, TYPE_NAME) \
  CALL_METHOD(TYPE, TYPE_NAME)         \
  GET_FIELD(TYPE, TYPE_NAME)           \
  CALL_STATIC_METHOD(TYPE, TYPE_NAME)  \
  GET_STATIC_FIELD(TYPE, TYPE_NAME)

TEMPLATE_SPEC(JavaObject, Object);
TEMPLATE_SPEC(JavaArray<JavaObject>, Object);
TEMPLATE_SPEC(jobject, Object);
TEMPLATE_SPEC(bool, Boolean);
TEMPLATE_SPEC(jboolean, Boolean);
TEMPLATE_SPEC(jbyte, Byte);
TEMPLATE_SPEC(jchar, Char);
TEMPLATE_SPEC(jshort, Short);
TEMPLATE_SPEC(jint, Int);
TEMPLATE_SPEC(long, Long);
TEMPLATE_SPEC(jlong, Long);
TEMPLATE_SPEC(jfloat, Float);
TEMPLATE_SPEC(jdouble, Double);

// for type `void`
template <> void JavaClass::__staticCall(JNIEnv *env, jmethodID methodId, ...) const {
  va_list args;
  va_start(args, methodId);
  env->CallStaticVoidMethodV(_jclazz.get(), methodId, args);
  va_end(args);
}

template <> void JavaObject::__call(JNIEnv *env, jmethodID methodId, ...) const {
  va_list args;
  va_start(args, methodId);
  env->CallVoidMethodV(_jobject.get(), methodId, args);
  va_end(args);
}

// for type `std::string`
template <> std::string JavaObject::__call(JNIEnv *env, jmethodID methodId, ...) const {
  va_list args;
  va_start(args, methodId);
  jstring jret = (jstring)env->CallObjectMethodV(_jobject.get(), methodId, args);
  va_end(args);
  return fromJString(jret, "", true);
}

template <> std::string JavaClass::__staticCall(JNIEnv *env, jmethodID methodId, ...) const {
  va_list args;
  va_start(args, methodId);
  jstring jret = (jstring)env->CallStaticObjectMethodV(_jclazz.get(), methodId, args);
  va_end(args);
  return fromJString(jret, "", true);
}

template <> std::string JavaObject::_field(JNIEnv *env, jfieldID fieldId) const {
  jstring jret = (jstring)env->GetObjectField(_jobject.get(), fieldId);
  return fromJString(jret, "", true);
}

template <> std::string JavaClass::_staticField(JNIEnv *env, jfieldID fieldId) const {
  jstring jret = (jstring)env->GetStaticObjectField(_jclazz.get(), fieldId);
  return fromJString(jret, "", true);
}

#pragma mark - Make Arg
jobject adaptArg(const JavaObject &javaObject) { return javaObject.getJObject(); }

JavaObject makeArg(const std::string &str) { return toJString(str); }

namespace env_util {
jclass findClass(JNIEnv *env, const std::string &classPath) throw(JniException) {
  jclass clazz = env->FindClass(classPath.c_str());
  if (clazz == nullptr) {
    throw JniException("Class not found: " + classPath);
  }
  JniException::checkException(env);
  return clazz;
}

jmethodID getMethodId(
    JNIEnv *env, jclass clazz, const std::string &methodName, const std::string &signature, bool isStatic) throw(JniException) {
  jmethodID methodId = nullptr;
  if (isStatic) {
    methodId = env->GetStaticMethodID(clazz, methodName.c_str(), signature.c_str());
  } else {
    methodId = env->GetMethodID(clazz, methodName.c_str(), signature.c_str());
  }
  if (methodId == nullptr) {
    std::ostringstream os;
    os << "Method `" << methodName << "` for `" << signature << "` not found.";
    throw JniException(os.str());
  }
  JniException::checkException(env);
  return methodId;
}

jfieldID getFieldId(JNIEnv *env, jclass clazz, const std::string &fieldName, const std::string &signature, bool isStatic) throw(
    JniException) {
  jfieldID fieldId = nullptr;
  if (isStatic) {
    fieldId = env->GetStaticFieldID(clazz, fieldName.c_str(), signature.c_str());
  } else {
    fieldId = env->GetFieldID(clazz, fieldName.c_str(), signature.c_str());
  }
  if (fieldId == nullptr) {
    std::ostringstream os;
    os << "Field `" << fieldName << "` for `" << signature << "` not found.";
    throw JniException(os.str());
  }
  JniException::checkException(env);
  return fieldId;
}
}
}  // namespace jnicpp11
