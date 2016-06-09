# JniCpp11

An Easy way to write JNI code in C++11.

JniCpp11 ease the pain from writing the JNI boilerplate code, generates JNI type signatures, and handles reference management such as `DeleteLocalRef` and `DeleteGlobalRef` for you, etc. 

Writing JNI code using JniCpp11 is almost as easy as writing their Java counter part, and less error prone. See [Usage](#usage) for details.

## Installation
It is recommended that you use clang rather than gcc.

### Cocos2d-x project
Clone this repo to cocos2d/external.

```bash
$ cd cocos2d/external
$ git submodule add <url-of-this-repo>
```
Then, add `jnicpp11_static` to `LOCAL_STATIC_LIBRARIES` and import its module in proj.android/jni/Android.mk.

```Makefile
# _COCOS_LIB_ANDROID_BEGIN
# ...
LOCAL_STATIC_LIBRARIES += jnicpp11_static
# _COCOS_LIB_ANDROID_END

# _COCOS_LIB_IMPORT_ANDROID_BEGIN
# ...
$(call import-module,JniCpp11)
# _COCOS_LIB_IMPORT_ANDROID_END
```
### General JNI project
Just add JniCpp11.h and JniCpp11.cpp to your project. Then, set the JavaVM in `JNI_OnLoad` function like this:

```cpp
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  Jni::setJvm(vm);
  return JNI_VERSION_1_4;
}
```

## <a name="usage"></a>Usage

***This README is still being written.***

> This project was initially created to remove dependencies on cocos2d-x from C++ wrappers of iOS/Android libraries. But now it can be used on more general JNI projects.

```cpp
using namespace jnicpp11;
```

### Finding a Java class
**JniCpp11**

```cpp
{
  auto clazz = JavaClass::getClass("android/os/Debug");

  if (clazz) {
    // class found
  } else {
    // class not found
  }	
}
// at this scope, the underlying globalRef of clazz is already deleted
```

This returns a globalRef of the Java class. You can store it for future usage. If you don't, the globalRef automatically deleted as the variable goes out of scope.

### Calling Java static methods
**JniCpp11**

```cpp
// If the JNI call fails, 0 is returned
auto nativeHeapSize = clazz.staticCall<jlong>("getNativeHeapSize", 0);

// If the JNI call fails, "Unknown" is returned in this case
std::string arg1 = "....";
auto runTimeStats = clazz.staticCall<std::string>("statName", "Unknown", arg1);

// void method calls does not need to provide a default return value since it does not return any value
clazz.staticCallVoid("dumpHprofDataDdms");

```

**Equivalent Java Code**

```java
import android.os.Debug;

long nativeHeapSize = Debug.getNativeHeapSize();

String arg1 = "...";
String runTimeStats = Debug.statName(arg1);

Debug.dumpHprofDataDdms();
```

### Calling Java instance methods
Examples to be written.

### Getting Java static fields
Examples to be written.

### Getting Java instance fields
Examples to be written.
