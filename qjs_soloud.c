#define STB_HEXWAVE_IMPLEMENTATION
#include "soloud_c.h"
#include "quickjs.h"
#include <stdlib.h>

#define countof(x) (sizeof(x)/sizeof((x)[0]))

#define FNSIG (JSContext *js, JSValueConst this_val, int argc, JSValue *argv)
#define GETSIG (JSContext *js, JSValueConst this_val)
#define SETSIG (JSContext *js, JSValueConst this_val, JSValue val)

#define JSCLASS(TYPE, FINALIZER) \
static JSClassID js_##TYPE##_class_id; \
static inline TYPE *js2##TYPE(JSContext *js, JSValue v) { \
  return JS_GetOpaque(v, js_##TYPE##_class_id); \
} \
static inline JSValue TYPE##2js(JSContext *js, TYPE *data) { \
  JSValue obj = JS_NewObjectClass(js, js_##TYPE##_class_id); \
  JS_SetOpaque(obj, data); \
  return obj; \
} \
static inline void js_##TYPE##_finalizer(JSRuntime *rt, JSValue val) { \
  FINALIZER(JS_GetOpaque(val, js_##TYPE##_class_id)); \
} \
static JSClassDef js_##TYPE##_class = { \
  #TYPE, \
  .finalizer = js_##TYPE##_finalizer \
}; \

#define JS_GETPROP(C, PROP, VAL, TYPE) \
{ \
  JSValue tmp = JS_GetPropertyStr(js, VAL, #PROP); \
  JS_To##TYPE(js, &C, tmp); \
  JS_FreeValue(js, tmp); \
} \

static double js2number(JSContext *js, JSValue v)
{
  double ret;
  JS_ToFloat64(js, &ret, v);
  return ret;
}

static JSValue number2js(JSContext *js, double num)
{
  return JS_NewFloat64(js, num);
}

static int js2bool(JSContext *js, JSValue v)
{
  int b;
  JS_ToInt32(js, &b, v);
  return b;
}

static JSValue bool2js(JSContext *js, int b)
{
  return JS_NewBool(js,b);
}

typedef unsigned int voice;

static Soloud *soloud;

JSCLASS(Wav, Wav_destroy)
JSCLASS(voice, free)
JSCLASS(Bus, Bus_destroy)

static JSValue js_soloud_make(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  soloud = Soloud_create();
  Soloud_initEx(soloud, SOLOUD_CLIP_ROUNDOFF, SOLOUD_AUTO, SOLOUD_AUTO, SOLOUD_AUTO, SOLOUD_AUTO);
  JSValue obj = JS_NewObject(js);
  JS_SetPropertyStr(js, obj, "channels", JS_NewFloat64(js, Soloud_getBackendChannels(soloud)));
  JS_SetPropertyStr(js, obj, "samplerate", JS_NewFloat64(js, Soloud_getBackendSamplerate(soloud)));
  return obj;
}

static JSValue js_soloud_play(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  Wav *wav = js2Wav(js,argv[0]);
  unsigned int *ret = malloc(sizeof(*ret));
  *ret = Soloud_play(soloud, wav);
  JSValue voice = voice2js(js,ret);
  return voice;
}

static JSValue js_soloud_mix(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  size_t len;
  void *data = JS_GetArrayBuffer(js, &len, argv[0]);
  Soloud_mix(soloud, data, js2number(js,argv[1]));
  return JS_UNDEFINED;
}

// Create a voice from a WAV file
static JSValue js_load_wav_mem(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  size_t len;
  void *data = JS_GetArrayBuffer(js, &len, argv[0]);
  Wav *wav = Wav_create();
  if (Wav_loadMemEx(wav, data, len, 1, 1)) {
    Wav_destroy(wav);
    return JS_ThrowReferenceError(js, "buffer data not wav data");
  }
  return Wav2js(js, wav);
}

// Create a voice from pure PWM data
static JSValue js_load_pwm(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  size_t len;
  void *data = JS_GetArrayBuffer(js, &len, argv[0]);
  Wav *wav = Wav_create();
  Wav_loadRawWaveEx(wav, data, len, js2number(js,argv[1]), js2number(js,argv[2]), 1, 1);
  return Wav2js(js, wav);
}

static JSValue js_soloud_profile(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  JSValue prof = JS_NewObject(js);
  JS_SetPropertyStr(js, prof, "active_voices", JS_NewFloat64(js, Soloud_getActiveVoiceCount(soloud)));
  JS_SetPropertyStr(js, prof, "voices", JS_NewFloat64(js, Soloud_getVoiceCount(soloud)));
}

static const JSCFunctionListEntry js_soloud_funcs[] = {
  JS_CFUNC_DEF("init", 3, js_soloud_make),
  JS_CFUNC_DEF("mix", 1, js_soloud_mix),
  JS_CFUNC_DEF("load_pwm", 3, js_load_pwm),
  JS_CFUNC_DEF("load_wav_mem", 1, js_load_wav_mem),
  JS_CFUNC_DEF("play", 1, js_soloud_play),
  JS_CFUNC_DEF("profile", 0, js_soloud_profile),
};

static const JSCFunctionListEntry js_Wav_funcs[] = {
};

#define SOLOUD_GETSET(ENTRY, TYPE) \
static JSValue js_voice_set_##ENTRY (JSContext *js, JSValueConst self, JSValue val) { \
  unsigned int voice = *js2voice(js, self); \
  Soloud_set##ENTRY(soloud, voice, js2##TYPE(js, val)); \
  return JS_UNDEFINED; \
} \
static JSValue js_voice_get_##ENTRY (JSContext *js, JSValueConst self) { \
  unsigned int voice = *js2voice(js,self); \
  return TYPE##2js(js, Soloud_get##ENTRY(soloud, voice)); \
} \

static JSValue js_voice_seek(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  unsigned int voice = *js2voice(js, self);
  Soloud_seek(soloud, voice, js2number(js, argv[0]));
  return JS_UNDEFINED;
}

static JSValue js_voice_stop(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  unsigned int voice = *js2voice(js, self);
  Soloud_stop(soloud, voice);
  return JS_UNDEFINED;
}

static JSValue js_voice_setInaudibleBehavior(JSContext *js, JSValueConst self, int argc, JSValue *argv)
{
  unsigned int voice = *js2voice(js, self);
  int mustTick = js2bool(js, argv[0]);
  int kill = js2bool(js, argv[1]);
  Soloud_setInaudibleBehavior(soloud, voice, mustTick, kill);
  return JS_UNDEFINED;
}

SOLOUD_GETSET(Volume, number);
SOLOUD_GETSET(Pan, number)
SOLOUD_GETSET(Samplerate, number)
SOLOUD_GETSET(RelativePlaySpeed, number)
SOLOUD_GETSET(LoopPoint, number)
SOLOUD_GETSET(Looping, bool)
SOLOUD_GETSET(AutoStop, bool)
SOLOUD_GETSET(ProtectVoice, bool)

static const JSCFunctionListEntry js_voice_funcs[] = {
  JS_CFUNC_DEF("seek", 1, js_voice_seek),
  JS_CFUNC_DEF("stop", 0, js_voice_stop),
  JS_CGETSET_DEF("volume", js_voice_get_Volume, js_voice_set_Volume),
  JS_CGETSET_DEF("pan", js_voice_get_Pan, js_voice_set_Pan),
  JS_CGETSET_DEF("samplerate", js_voice_get_Samplerate, js_voice_set_Samplerate),
  JS_CGETSET_DEF("relativePlaySpeed", js_voice_get_RelativePlaySpeed, js_voice_set_RelativePlaySpeed),
  JS_CGETSET_DEF("loopPoint", js_voice_get_LoopPoint, js_voice_set_LoopPoint),
  JS_CGETSET_DEF("loop", js_voice_get_Looping, js_voice_set_Looping),
  JS_CGETSET_DEF("autoStop", js_voice_get_AutoStop, js_voice_set_AutoStop),
  JS_CGETSET_DEF("protect", js_voice_get_ProtectVoice, js_voice_set_ProtectVoice),    
};

static const JSCFunctionListEntry js_Bus_funcs[] = {
  
};

#define INITCLASS(TYPE) \
JS_NewClassID(&js_##TYPE##_class_id); \
JS_NewClass(JS_GetRuntime(js), js_##TYPE##_class_id, &js_##TYPE##_class); \
JSValue TYPE##_proto = JS_NewObject(js); \
JS_SetPropertyFunctionList(js, TYPE##_proto, js_##TYPE##_funcs, countof(js_##TYPE##_funcs)); \
JS_SetClassProto(js, js_##TYPE##_class_id, TYPE##_proto); \

JSValue js_soloud_use(JSContext *js)
{
  INITCLASS(Wav)
  INITCLASS(voice)
  INITCLASS(Bus)
  JSValue export = JS_NewObject(js);
  JS_SetPropertyFunctionList(js, export, js_soloud_funcs, sizeof(js_soloud_funcs)/sizeof(JSCFunctionListEntry));
  return export;
}

static int js_soloud_init(JSContext *js, JSModuleDef *m) {
  js_soloud_use(js);
  JS_SetModuleExportList(js, m, js_soloud_funcs, sizeof(js_soloud_funcs)/sizeof(JSCFunctionListEntry));
  return 0;
}

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_soloud
#endif

JSModuleDef *JS_INIT_MODULE(JSContext *ctx, const char *module_name) {
  JSModuleDef *m = JS_NewCModule(ctx, module_name, js_soloud_init);
  if (!m) return NULL;
  JS_AddModuleExportList(ctx, m, js_soloud_funcs, sizeof(js_soloud_funcs)/sizeof(JSCFunctionListEntry));
  return m;
}
