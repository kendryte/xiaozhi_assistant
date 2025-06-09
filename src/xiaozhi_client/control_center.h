#ifndef ControlCenter_H
#define ControlCenter_H
typedef enum ListeningMode {
    kListeningModeAutoStop,
    kListeningModeManualStop,
    kListeningModeAlwaysOn // 需要 AEC 支持
} ListeningMode;

// 定义设备状态枚举类型
typedef enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateFatalError
} DeviceState;

typedef enum tag_SpeechInteractionMode {
    kSpeechInteractionModeAuto,
    kSpeechInteractionModeManual,
    kSpeechInteractionModeRealtime,
    kSpeechInteractionModeAutoWithWakeupWord,
} SpeechInteractionMode;

typedef enum tag_WakeupMethodMode {
    kWakeupMethodModeUnknown,
    kWakeupMethodModeVoice,
    kWakeupMethodModeVideo,
} WakeupMethodMode;

typedef void (*server_audio_data_callback)(const unsigned char* data, int length);
typedef void (*audio_tts_state_callback)(int tts_state);
typedef void (*ws_work_state_callback)(bool work_state);

int  init_device(server_audio_data_callback callback,audio_tts_state_callback tts_callback,ws_work_state_callback ws_state_callback);
int  active_device();
int  connect_to_xiaozhi_server();
int  abort_cur_session();
int  send_audio(const unsigned char *buffer, int size, void *user_data);

#endif // ControlCenter_H