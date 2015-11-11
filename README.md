
## Introduction

What is DynamicAPK?

DynamicAPK is a solution that contains framework, tool and configuration to implement multi apk/dex dynamic loading. It can help reorganize Android project configuration and development model to achieve sub-projects parallel development (in the form of android studio module), while supporting hot fix (repairing online bug), on-demand loading seldom-used modules. All dynamically loaded modules not only contain code but also contain resources if you need.

DynamicAPK is already uesed in Ctrip Android App (Simplified Chinese Version). Ctrip is  the biggest online travel agency in China, while 72 percent of orders are from App.

## Benefits

* Less transformation effort (no activity/fragment/resource proxy stuff)
	
	DynamicAPK doesn't need activity or fragment proxy to manage their life cycle. Modules' resources are processed by modified aapt, so resource reference in R.java is not different with normal Android project. Developers can maintain their original development paradigm.	
	
* Parallel development

* Speed up compilation

* Speed up app booting

	MultiDex solution offered by Google will execute dex decompression, dexopt, load operation in the main thread. That means a very long process, while users will see significant long black screen and more likely to encounter the ANR. By DynamicAPK, app loads only the necessary modules, other modules are loaded on demand. 
	
* Hot fix (code and resource)

* On-demand module (code and resource) downloading and loading 

## Comparasion

* [DynamicLoadApk](https://github.com/singwhatiwanna/dynamic-load-apk)
	
	Heavy develpment paradigm transformation: use "that" instead of "this", activity should inherit from their proxy avtivity (The proxy activity manage life cycle).
	
	Restrictions of starting activity within module apk.
	
	Doesn't support Service and BroadcastReceiver.

* [AndroidDynamicLoader](https://github.com/mmin18/AndroidDynamicLoader)

	Heavy develpment paradigm transformation: 
	
	Changes the usage of resources: `MyResources.getResource(Me.class)` instead of `context.getResources()`. 
	
	Use Fragment as UI container, each page is implemented in Fragment instead of Activity. So you need use URL mapping to start new page.

* [android-pluginmgr](https://github.com/houkx/android-pluginmgr)
	
	Not tested in released App.
	
	Doesn't support Service and BroadcastReceiver.

* [DroidPlugin](https://github.com/Qihoo360/DroidPlugin) from Qihu360
	
	Very interesting framework! DroidPlugin can start totally independent app (not installed) in your app. The features are more suitable for Qihu360 security app because the bundle apk is totally irrelevant to host apk. 
	
	Doesn't support custom nitification.  

## Implementation

[Android Build Process](http://7xns6i.com1.z0.glb.clouddn.com/ctrip-pluggable/android_build_process.png "Android Build Process")

We focus on aapt, javac, proguard and dex process. The key of dynamic loading is about two things:

### Code compilation and loading

Java compilation is nothing special, while class loading needs some hacking. Android's DexClassLoader has some restrictions, so we use Android's system PathClassLoader. PathClassLoader has a member pathList, as the name suggests it is essentially a List to  load classes from each dex path in the list at runtime. So we can add our dynamically loaded dex at the head of the list. In fact, Google's official MultiDex library is also implemented by the method. The following snippet shows the details:

MultiDex.java

```java
private static void install(ClassLoader loader, List<File> additionalClassPathEntries,
     File optimizedDirectory)
             throws IllegalArgumentException, IllegalAccessException,
             NoSuchFieldException, InvocationTargetException, NoSuchMethodException {
    /* The patched class loader is expected to be a descendant of
    * dalvik.system.BaseDexClassLoader. We modify its
    * dalvik.system.DexPathList pathList field to append additional DEX
    * file entries.
    */
    Field pathListField = findField(loader, "pathList");
    Object dexPathList = pathListField.get(loader);
    expandFieldArray(dexPathList, "dexElements", makeDexElements(dexPathList,
         new ArrayList<File>(additionalClassPathEntries), optimizedDirectory));
}
```

For different versions of Android, class loading has a slightly different way. Reference [MultiDex Source] (https://android.googlesource.com/platform/frameworks/multidex/+/d79604bd38c101b54e41745f85ddc2e04d978af2/library/src/android/support/multidex/MultiDex.java).

### Resource compilation and loading

Resource compilation is proceesed by Android tool: aapt, which is located in `<SDK> / build-tools / <buildToolsVersion> / aapt`, with many [command line parameter] (http: //7xns6i.com1.z0 .glb.clouddn.com / ctrip-pluggable / aapt.txt "aapt Command Line Reference"). Some of them deserve special attention:

- `-I Add an existing package to base include set`

	This parameter is to add an existing package in the dependency path. In Android, the compilation of resources also need rely on android.jar. "android.jar" is not an ordinary jar package, which contains the existing SDK library class, compiled resources  and resource index file (resources.arsc). Similarly, we can also use this parameter references an existing apk packages as dependencies resources to participate in the compilation. 

- `-G A file to output proguard options into.`

	In resource compilation, component class and method references will result in runtime reflection invocation, so this kind of symbol can not be confused. -G parameter will derive classes and interfaces found in the resource compilation process that must be kept. It will participate to the confusion in the late stage as an additional configuration file.

- `-J Specify where to output R.java resource constant definitions`

	In Android, all resources will be generated as corresponding constant ID, the ID will be merged to R.java file. Resource ID in R.java is a four-byte int type. Actually it consists of three fields. The first byte represents the package, the second byte represents type, three and four bytes represent real ID. E.g:
    

	```
	//android.jar resources, PackageID is 0x01
	public static final int cancel = 0x01040000;
    
	// User app resources, PackageID is 0x7F
	public static final int zip_code = 0x7f090f2e;
	```

	We modifed aapt to provide each module different PackageID, so there will be no conflict.

- To add new aapt `--apk-module` parameter.

	As previously mentioned, we specified for each module project unique PackageID field, so we can find where to find and load resources. In the resource loading section there will be more details.
   
- To add new aapt `--public-R-path` parameter.

	Android system resources can be referenced by its fully qualified name `android.R`  to refer specific source. If we use `base.package.name.R` for modules to refer public app common resources, that means we need modify every existed resource reference code. It's error-prone and less transparent in the future development. We add `--public-R-path` parameter to specify `base.R`'s location and make copy of common resource ID into modules' R.java.
	

Resource loading is processed by AssetManager and Resources class. We can access them in the Context.

Context.java

```java
/** Return an AssetManager instance for your application's package. */
public abstract AssetManager getAssets();

/** Return a Resources instance for your application's package. */
public abstract Resources getResources();
```

They are two abstract methods, implementation is in ContextImpl class. After initialization of ContextImpl class objects, each subclass of Context such as Activity, Service and other components can access resources by these two methods.

ContextImpl.java

```java
private final Resources mResources;

@Override
public AssetManager getAssets() {
   return getResources().getAssets();
}

@Override
public Resources getResources() {
   return mResources;
}
```

Since we allocate PackageID (the first byte of resource ID) by aapt to know where to find resource's apk, we override these two methods to find specific resource.

And there is a hidden method addAssetPath in AssetManager, so we can add a resource path to AssetManager.

```
/ **
* Add an additional set of assets to the asset manager. This can be
* Either a directory or ZIP file. Not for use by applications. Returns
* The cookie of the added asset, or 0 on failure.
* {hide}
* /
public final int addAssetPath(String path) {
   synchronized(this) {
       int res = addAssetPathNative(path);
       makeStringBlocks(mStringBlocks);
       return res;
   }
}
```

We just need to reflect this method, then add all apk's location to AssetManager. AssetManager will finish the resource loading by compiled resources.arsc resources within apk.

To achieve "seamless" experience, we need last step: using the Instrumentation to take over all creation of Activity , Service and other components. Activity, Service and other system components will be loaded in the main thread by android.app.ActivityThread. ActivityThread class has a member mInstrumentation, that is responsible for creating Activity and other operations. So it's the best candidate for loading our modified resource class. Every time the system creates Activity, we replace its mResources by our DelegateResources that will know how to load resources. Done!


## Usage

### aapt

- Use parameter --apk-module to allocate packageID
	
	E.g: ex: aapt ...... --apk-module 0x58 （PackageID in ResourceID is 0x58）
	
- Use parameter --public-R-path to merge R.java (RMerge.cpp)  

	Then the output R file contains the base apk R.java and the module apk R.java.

### Build
- $ git clone https://github.com/CtripMobile/DynamicAPK.git
- $ cd DynamicAPK/
- $ gradle assembleRelease bundleRelease repackAll
- Release APK in /build-outputs/***-release-final.apk


# Simplified Chinese Version


## 介绍

DynamicAPK是一套用于实现多dex/apk加载的解决方案。它可以帮助你重新组织Android工程的配置和开发模式，实现多个子工程并行开发（以android studio module的形式），同时支持hot fix（在线修复有问题的功能）, 插件式载入不常用的功能（下载插件后再载入）。所有动态加载的插件不仅包含代码，也可以包含资源（资源的动态加载比代码要麻烦很多），因此是以APK形式实现的。

DynamicAPK已经在携程旅行Android App中使用，欢迎关注携程移动技术公众号：CtripMobile

## 价值

* 更少的迁移成本（无需做任何activity/fragment/resource的proxy实现）
	
	DynamicAPK不需要实现任何activity或fragment proxy来管理他们的生命周期。修改后的aapt会处理插件中的资源，因此R.java中的资源引用和普通Android工程没有区别。开发者可以保持原有的开发范式，无需做特殊的更改。
	
* 并发开发

* 提升编译速度

* 提升启动速度

	Google提供的MultiDex方案，会在主线程中执行所有dex的解压、dexopt、加载操作，这是一个非常漫长的过程，用户会明显的看到长久的黑屏，更容易造成主线程的ANR，导致首次启动初始化失败。DynamicAPK可以在App启动时仅加载必须的模块，其他模块按需加载。	

* Hot fix (包含代码和资源)

* 按需下载和加载任意功能模块(包含代码和资源)

## 对比

* [DynamicLoadApk](https://github.com/singwhatiwanna/dynamic-load-apk)
	
	迁移成本很重：需要使用『that』而不是『this』，所有activity都需要继承自proxy avtivity（proxy avtivity负责管理所有activity的生命周期）。
	
	无法启动apk内部的activity。
	
	不支持Service和BroadcastReceiver。

* [AndroidDynamicLoader](https://github.com/mmin18/AndroidDynamicLoader)

	迁移成本很重：
	
	使用资源时要用`MyResources.getResource(Me.class)`而不是`context.getResources()`
	
	使用Fragment作为UI容器，所有每个页面都是使用Fragment而不是Activity，需要使用URL mapping才能实现页面跳转。
	
* [android-pluginmgr](https://github.com/houkx/android-pluginmgr)
	
	未经过生产环境App测试。
	
	不支持Service和BroadcastReceiver。

* [DroidPlugin](https://github.com/Qihoo360/DroidPlugin) from 奇虎360
	
	非常有趣的框架！DroidPlugin能够在一个App内启动一个没有安装的App。这个特性可能更适合360的安全产品，因为被启动的App和宿主App完全没有任何关联，相互间不能支持资源和代码调用。
	
	不支持自定义推送栏。
	
## 实现细节

更深入的分析文章详见 [InfoQ -《携程Android App插件化和动态加载实践》](http://www.infoq.com/cn/articles/ctrip-android-dynamic-loading)

## 使用方法

### aapt

- 使用参数 --apk-module 来分配PackageID
	
	E.g: ex: aapt ...... --apk-module 0x58 （ResourceID的PackageID定义为0x58）
	
- 使用个参数 --public-R-path 来合并R.java (实现文件在RMerge.cpp中)  

	生成的R.java文件会合并基础APK和模块APK中的R.java。

### Build
- $ git clone https://github.com/CtripMobile/DynamicAPK.git
- $ cd DynamicAPK/
- $ gradle assembleRelease bundleRelease repackAll
- Release APK in /build-outputs/***-release-final.apk