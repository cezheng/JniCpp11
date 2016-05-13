# JniCpp11

An Easy way to write JNI code in C++11.

This project was initially created to remove dependencies on cocos2d-x from C++ wrappers of iOS/Android libraries. But now it can be used on more general JNI projects.

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
