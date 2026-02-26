# HeliBoard Adapter

Reference adapter for integrating the libswipetype with [HeliBoard](https://github.com/Helium314/HeliBoard).

## Integration Approaches

### Approach 1: Java-Level Integration (Recommended for MVP)

Add this adapter as a Gradle dependency in your HeliBoard fork:

1. Add to HeliBoard's `settings.gradle`:
   ```gradle
   include ':swipetype-android', ':adapters:heliboard'
   ```

2. Add dependency in HeliBoard's `app/build.gradle`:
   ```gradle
   implementation project(':adapters:heliboard')
   ```

3. In HeliBoard's `LatinIME.java`, initialize the adapter:
   ```java
   HeliboardSwipeTypeAdapter glideAdapter = new HeliboardSwipeTypeAdapter(
       new HeliboardSwipeTypeAdapter.CandidateCallback() {
           @Override
           public void onCandidates(String[] words, int[] scores, int count) {
               // Convert to SuggestedWordInfo and show in suggestion bar
           }
           @Override
           public void onError(String message) {
               Log.e("LatinIME", "Glide error: " + message);
           }
       }
   );
   glideAdapter.initialize(this, openDictionaryStream());
   ```

4. In HeliBoard's gesture input handler, route to the adapter:
   ```java
   if (isGestureInput) {
       glideAdapter.onGestureInput(
           inputPointers.getXCoordinates(),
           inputPointers.getYCoordinates(),
           inputPointers.getTimes(),
           inputPointers.getPointerSize()
       );
   }
   ```

### Approach 2: Drop-in .so Replacement (Future)

Build the library as `libjni_latinime.so` and place it in HeliBoard's files directory.
This requires the `HeliboardJNIBridge.cpp` to be completed with full JNI registration.

## HeliBoard Version Compatibility

This adapter is designed against HeliBoard's main branch as of 2026-02-18. Key interfaces observed:

- `BinaryDictionary.getSuggestionsNative()` — gesture/typing suggestion entry point
- `ProximityInfo.setProximityInfoNative()` — keyboard layout setup
- `NativeSuggestOptions` — gesture mode flag (index 0)
- `InputPointers` — touch coordinate container

If HeliBoard changes these interfaces, the adapter may need updating. See the version compatibility section in the main README.
