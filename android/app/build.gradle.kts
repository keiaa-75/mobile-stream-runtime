plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "dev.msr.engine"
    compileSdk = 35

    defaultConfig {
        applicationId = "dev.msr.engine"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1-prototype"
        ndk { abiFilters += listOf("arm64-v8a") }
    }

    buildTypes {
        debug { isDebuggable = true }
    }

    // Framework-only UI (no AndroidX): fewer moving parts for a first APK.
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}
