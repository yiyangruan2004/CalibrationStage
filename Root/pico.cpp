#include "pico.h"

#include <algorithm>


Pico::Pico(QObject *parent)
    : Device(parent)
#ifndef Q_OS_WASM
    , handle(0)
#endif
{}

DeviceState Pico::connect(bool connection){
#ifdef Q_OS_WASM
    deviceState = connection ? online : offline;
    if (connection) {
        config();
    }
    qDebug() << "pico.simulation.connect:" << deviceState;
    return deviceState;
#else
    qDebug() << "pico.connect";
    PICO_STATUS status;
    if (connection){
        status = ps5000aOpenUnit(&handle, nullptr, PS5000A_DR_16BIT);
        if (status != PICO_OK) {
            qWarning() << "Restart picoscope \n Picoscope failed to OpenUnit \n" << picoStatusToString(status);
            deviceState = offline;
        }else{
            deviceState = online;
            config();
        }
    }else{
        status = ps5000aCloseUnit(handle);
        if (status != PICO_OK) {
            qWarning() << "Improper picoscope connection \n" << picoStatusToString(status);
        }
        deviceState = offline;
    }

    qDebug() << "pico.online: " << deviceState;
    return deviceState;
#endif
}


DeviceState Pico::config(){
#ifdef Q_OS_WASM
    if (deviceState == offline) {
        qWarning() << "Attempt to configure simulated picoscope while offline";
        return deviceState;
    }
    deviceState = ready;
    qDebug() << "pico.simulation.config:" << deviceState;
    return deviceState;
#else
    if(deviceState == offline){
        qWarning() << "Attempt to configure picoscope while offline";
        return deviceState;
    }else{
        range = static_cast<PS5000A_RANGE>(range);
        PICO_STATUS status = ps5000aSetChannel(handle,PS5000A_CHANNEL_A,1,PS5000A_AC, static_cast<PS5000A_RANGE>(range), 0);
        if (status != PICO_OK) {
            qWarning() << "Restart picoscope \n Picoscope failed to SetChannel \n" << picoStatusToString(status);
            deviceState = error;
        }
        buffer.resize(samp);
        status = ps5000aSetDataBuffers(handle, PS5000A_CHANNEL_A, buffer.data(), nullptr, samp, 0, PS5000A_RATIO_MODE_NONE);
        if (status != PICO_OK) {
            qWarning() << "Restart picoscope \n Picoscope failed to SetDataBuffers \n" << picoStatusToString(status);
            deviceState = error;
        }
        status = ps5000aSetSimpleTrigger(handle, 1, PS5000A_EXTERNAL, 1000, PS5000A_RISING, 0, 0);
        if (status != PICO_OK) {
            qWarning() << "Restart picoscope \n Picoscope failed to SetSimpleTrigger \n" << picoStatusToString(status);
            deviceState = error;
        }
        float timeIntervalNs;
        int32_t maxSamples;
        status = ps5000aGetTimebase2(handle, timebase, samp, &timeIntervalNs, &maxSamples, 0);
        if (status != PICO_OK) {
            qWarning() << "Invalid timebase and sample frequency \n" << picoStatusToString(status);
            deviceState = error;
        }
    }
    if (deviceState == online){
        deviceState = ready;
    }
    qDebug() << "pico.config: " << deviceState;
    return deviceState;
#endif
}

DeviceState Pico::runBlock(){
#ifdef Q_OS_WASM
    if (deviceState != ready) {
        qWarning() << "Attempt to run simulated picoscope while not configured";
    }
    return deviceState;
#else
    PICO_STATUS status = ps5000aRunBlock(handle, 0, samp, timebase, nullptr, 0, nullptr, nullptr);
    if (status != PICO_OK) {
        qWarning() << "Restart picoscope \n Picoscope failed to RunBlock \n" << picoStatusToString(status);
        deviceState = error;
    }
    return deviceState;
#endif
}

Data Pico::read(){
#ifdef Q_OS_WASM
    Data data;
    if (deviceState != ready) {
        qWarning() << "Attempt to read simulated picoscope while not configured";
        return data;
    }

    const int sampleCount = std::max(samp, 1);
    constexpr double referenceSamples = 1000.0;
    constexpr double pi = 3.14159265358979323846;
    const double standardDeviation = 0.5;
    std::normal_distribution<double> noise(0.0, standardDeviation);
    const auto dampedPulse = [pi](double sample, double centre, double amplitude, double width) {
        const double delta = sample - centre;
        const double envelope = std::exp(-0.5 * (delta / width) * (delta / width));
        return amplitude * envelope * std::cos(2.0 * pi * 0.04 * delta);
    };

    data.points.reserve(sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample) {
        const double referenceSample = static_cast<double>(sample) * referenceSamples / sampleCount;
        const double primaryBurst = dampedPulse(referenceSample, 235.0 + simulationTimeShift, 84.0, 25.0);
        const double delayedEcho = dampedPulse(referenceSample, 620.0 + simulationTimeShift, 10.0, 22.0);
        const double value = noise(simulationGenerator) + simulationGain * (primaryBurst + delayedEcho);
        data.peak = std::max(data.peak, std::abs(value));
        data.points.append(QPointF(sample + offset, value));
    }
    return data;
#else
    Data data;
    if (deviceState != ready){
        //Generate fakedata
        // static std::random_device rd;
        // static std::mt19937 gen(rd());
        // double limit = volt[range] / 2.0 * 1000.0;
        // std::uniform_real_distribution<double> dist(-limit, limit);

        // for (uint32_t i = 0; i < samp; ++i) {
        //     double value = dist(gen);
        //     if (value > peak){
        //         peak = value;
        //         data.tof = i;
        //     }
        //     if(value < valley){
        //         valley = value;
        //         data.tof = i;
        //     }
        //     data.points.append(QPointF(i, value));
        // }
        qWarning() << "Attempt to read picoscope while not configured";
        return data;
    }else{
        int16_t ready = 0;
        QElapsedTimer timer;
        timer.start();
        while (!ready) {
            ps5000aIsReady(handle, &ready);
            if (timer.elapsed() > 1000) {
                qWarning() << "Picoscope not receiving trigger";
                return {0};
            }
            QThread::msleep(10);
        }

        uint32_t samplesReturned = samp;
        PICO_STATUS status = ps5000aGetValues(handle, 0, &samplesReturned, 1, PS5000A_RATIO_MODE_NONE, 0, nullptr);
        if (status != PICO_OK) {
            return {0};
        }
        const double maxADC = 32767.0;
        for (uint32_t i = 0; i < samplesReturned; ++i){
            double voltage = (static_cast<double>(buffer[i]) / maxADC) * volt[range];
            if (abs(voltage) > abs(data.peak)){
                data.peak = voltage;
            }
            data.points.append(QPointF(i, voltage));
        }
    }
    return data;
#endif
}

void Pico::setSimulationGain(double gain){
#ifdef Q_OS_WASM
    simulationGain = std::clamp(gain, 0.0, 1.0);
#else
    Q_UNUSED(gain)
#endif
}

void Pico::setSimulationTimeShift(double timeShift)
{
#ifdef Q_OS_WASM
    simulationTimeShift = std::clamp(timeShift, 0.0, 160.0);
#else
    Q_UNUSED(timeShift)
#endif
}

Pico::~Pico(){
    connect(false);
}



#ifndef Q_OS_WASM
const char* Pico::picoStatusToString(PICO_STATUS status){
    switch (status)
    {
    // Success
    case PICO_OK:
        return "PICO_OK: The PicoScope is functioning correctly.";

    // General Errors (0x00000001 - 0x00000060)
    case PICO_MAX_UNITS_OPENED:
        return "PICO_MAX_UNITS_OPENED: An attempt has been made to open more than the maximum units.";
    case PICO_MEMORY_FAIL:
        return "PICO_MEMORY_FAIL: Not enough memory could be allocated on the host machine.";
    case PICO_NOT_FOUND:
        return "PICO_NOT_FOUND: No Pico Technology device could be found.";
    case PICO_FW_FAIL:
        return "PICO_FW_FAIL: Unable to download firmware.";
    case PICO_OPEN_OPERATION_IN_PROGRESS:
        return "PICO_OPEN_OPERATION_IN_PROGRESS: The driver is busy opening a device.";
    case PICO_OPERATION_FAILED:
        return "PICO_OPERATION_FAILED: An unspecified failure occurred.";
    case PICO_NOT_RESPONDING:
        return "PICO_NOT_RESPONDING: The PicoScope is not responding to commands from the PC.";
    case PICO_CONFIG_FAIL:
        return "PICO_CONFIG_FAIL: The configuration information in the PicoScope is corrupt or missing.";
    case PICO_KERNEL_DRIVER_TOO_OLD:
        return "PICO_KERNEL_DRIVER_TOO_OLD: The picopp.sys file is too old to be used with the device driver.";
    case PICO_EEPROM_CORRUPT:
        return "PICO_EEPROM_CORRUPT: The EEPROM has become corrupt, so the device will use a default setting.";
    case PICO_OS_NOT_SUPPORTED:
        return "PICO_OS_NOT_SUPPORTED: The operating system on the PC is not supported by this driver.";
    case PICO_INVALID_HANDLE:
        return "PICO_INVALID_HANDLE: There is no device with the handle value passed.";
    case PICO_INVALID_PARAMETER:
        return "PICO_INVALID_PARAMETER: A parameter value is not valid.";
    case PICO_INVALID_TIMEBASE:
        return "PICO_INVALID_TIMEBASE: The timebase is not supported or is invalid.";
    case PICO_INVALID_VOLTAGE_RANGE:
        return "PICO_INVALID_VOLTAGE_RANGE: The voltage range is not supported or is invalid.";
    case PICO_INVALID_CHANNEL:
        return "PICO_INVALID_CHANNEL: The channel number is not valid on this device or no channels have been set.";
    case PICO_INVALID_TRIGGER_CHANNEL:
        return "PICO_INVALID_TRIGGER_CHANNEL: The channel set for a trigger is not available on this device.";
    case PICO_INVALID_CONDITION_CHANNEL:
        return "PICO_INVALID_CONDITION_CHANNEL: The channel set for a condition is not available on this device.";
    case PICO_NO_SIGNAL_GENERATOR:
        return "PICO_NO_SIGNAL_GENERATOR: The device does not have a signal generator.";
    case PICO_STREAMING_FAILED:
        return "PICO_STREAMING_FAILED: Streaming has failed to start or has stopped without user request.";
    case PICO_BLOCK_MODE_FAILED:
        return "PICO_BLOCK_MODE_FAILED: Block failed to start - a parameter may have been set wrongly.";
    case PICO_NULL_PARAMETER:
        return "PICO_NULL_PARAMETER: A parameter that was required is NULL.";
    case PICO_ETS_MODE_SET:
        return "PICO_ETS_MODE_SET: The current functionality is not available while using ETS capture mode.";
    case PICO_DATA_NOT_AVAILABLE:
        return "PICO_DATA_NOT_AVAILABLE: No data is available from a run block call.";
    case PICO_STRING_BUFFER_TO_SMALL:
        return "PICO_STRING_BUFFER_TO_SMALL: The buffer passed for the information was too small.";
    case PICO_ETS_NOT_SUPPORTED:
        return "PICO_ETS_NOT_SUPPORTED: ETS is not supported on this device.";
    case PICO_AUTO_TRIGGER_TIME_TO_SHORT:
        return "PICO_AUTO_TRIGGER_TIME_TO_SHORT: The auto trigger time is less than the time it will take to collect the pre-trigger data.";
    case PICO_BUFFER_STALL:
        return "PICO_BUFFER_STALL: The collection of data has stalled as unread data would be overwritten.";
    case PICO_TOO_MANY_SAMPLES:
        return "PICO_TOO_MANY_SAMPLES: Number of samples requested is more than available in the current memory segment.";
    case PICO_TOO_MANY_SEGMENTS:
        return "PICO_TOO_MANY_SEGMENTS: Not possible to create number of segments requested.";
    case PICO_PULSE_WIDTH_QUALIFIER:
        return "PICO_PULSE_WIDTH_QUALIFIER: A null pointer has been passed in the trigger function or one of the parameters is out of range.";
    case PICO_DELAY:
        return "PICO_DELAY: One or more of the hold-off parameters are out of range.";
    case PICO_SOURCE_DETAILS:
        return "PICO_SOURCE_DETAILS: One or more of the source details are incorrect.";
    case PICO_CONDITIONS:
        return "PICO_CONDITIONS: One or more of the conditions are incorrect.";
    case PICO_USER_CALLBACK:
        return "PICO_USER_CALLBACK: The driver's thread is currently in the Ready callback function and therefore the action cannot be carried out.";
    case PICO_DEVICE_SAMPLING:
        return "PICO_DEVICE_SAMPLING: An attempt is being made to get stored data while streaming.";
    case PICO_NO_SAMPLES_AVAILABLE:
        return "PICO_NO_SAMPLES_AVAILABLE: Data is unavailable because a run has not been completed.";
    case PICO_SEGMENT_OUT_OF_RANGE:
        return "PICO_SEGMENT_OUT_OF_RANGE: The memory segment index is out of range.";
    case PICO_BUSY:
        return "PICO_BUSY: The device is busy so data cannot be returned yet.";
    case PICO_STARTINDEX_INVALID:
        return "PICO_STARTINDEX_INVALID: The start time to get stored data is out of range.";
    case PICO_INVALID_INFO:
        return "PICO_INVALID_INFO: The information number requested is not a valid number.";
    case PICO_INFO_UNAVAILABLE:
        return "PICO_INFO_UNAVAILABLE: The handle is invalid so no information is available about the device.";
    case PICO_INVALID_SAMPLE_INTERVAL:
        return "PICO_INVALID_SAMPLE_INTERVAL: The sample interval selected for streaming is out of range.";
    case PICO_TRIGGER_ERROR:
        return "PICO_TRIGGER_ERROR: ETS is set but no trigger has been set. A trigger setting is required for ETS.";
    case PICO_MEMORY:
        return "PICO_MEMORY: Driver cannot allocate memory.";
    case PICO_SIG_GEN_PARAM:
        return "PICO_SIG_GEN_PARAM: Incorrect parameter passed to the signal generator.";
    case PICO_SHOTS_SWEEPS_WARNING:
        return "PICO_SHOTS_SWEEPS_WARNING: Conflict between the shots and sweeps parameters sent to the signal generator.";
    case PICO_SIGGEN_TRIGGER_SOURCE:
        return "PICO_SIGGEN_TRIGGER_SOURCE: A software trigger has been sent but the trigger source is not a software trigger.";
    case PICO_AUX_OUTPUT_CONFLICT:
        return "PICO_AUX_OUTPUT_CONFLICT: A SetTrigger call has found a conflict between the trigger source and the AUX output enable.";
    case PICO_AUX_OUTPUT_ETS_CONFLICT:
        return "PICO_AUX_OUTPUT_ETS_CONFLICT: ETS mode is being used and AUX is set as an input.";
    case PICO_WARNING_EXT_THRESHOLD_CONFLICT:
        return "PICO_WARNING_EXT_THRESHOLD_CONFLICT: Attempt to set different EXT input thresholds set for signal generator and oscilloscope trigger.";
    case PICO_WARNING_AUX_OUTPUT_CONFLICT:
        return "PICO_WARNING_AUX_OUTPUT_CONFLICT: A SetTrigger function has set AUX as an output and the signal generator is using it as a trigger.";
    case PICO_SIGGEN_OUTPUT_OVER_VOLTAGE:
        return "PICO_SIGGEN_OUTPUT_OVER_VOLTAGE: The combined peak-to-peak voltage and the analog offset voltage exceed the maximum voltage the signal generator can produce.";
    case PICO_DELAY_NULL:
        return "PICO_DELAY_NULL: NULL pointer passed as delay parameter.";
    case PICO_INVALID_BUFFER:
        return "PICO_INVALID_BUFFER: The buffers for overview data have not been set while streaming.";
    case PICO_SIGGEN_OFFSET_VOLTAGE:
        return "PICO_SIGGEN_OFFSET_VOLTAGE: The analog offset voltage is out of range.";
    case PICO_SIGGEN_PK_TO_PK:
        return "PICO_SIGGEN_PK_TO_PK: The analog peak-to-peak voltage is out of range.";
    case PICO_CANCELLED:
        return "PICO_CANCELLED: A block collection has been cancelled.";
    case PICO_SEGMENT_NOT_USED:
        return "PICO_SEGMENT_NOT_USED: The segment index is not currently being used.";
    case PICO_INVALID_CALL:
        return "PICO_INVALID_CALL: The wrong GetValues function has been called for the collection mode in use.";
    case PICO_GET_VALUES_INTERRUPTED:
        return "PICO_GET_VALUES_INTERRUPTED: The get values operation was interrupted.";
    case PICO_NOT_USED:
        return "PICO_NOT_USED: The function is not available.";
    case PICO_INVALID_SAMPLERATIO:
        return "PICO_INVALID_SAMPLERATIO: The aggregation ratio requested is out of range.";
    case PICO_INVALID_STATE:
        return "PICO_INVALID_STATE: Device is in an invalid state.";
    case PICO_NOT_ENOUGH_SEGMENTS:
        return "PICO_NOT_ENOUGH_SEGMENTS: The number of segments allocated is fewer than the number of captures requested.";
    case PICO_DRIVER_FUNCTION:
        return "PICO_DRIVER_FUNCTION: A driver function has already been called and not yet finished.";
    case PICO_RESERVED:
        return "PICO_RESERVED: Not used.";
    case PICO_INVALID_COUPLING:
        return "PICO_INVALID_COUPLING: An invalid coupling type was specified in SetChannel.";
    case PICO_BUFFERS_NOT_SET:
        return "PICO_BUFFERS_NOT_SET: An attempt was made to get data before a data buffer was defined.";
    case PICO_RATIO_MODE_NOT_SUPPORTED:
        return "PICO_RATIO_MODE_NOT_SUPPORTED: The selected downsampling mode (used for data reduction) is not allowed.";
    case PICO_RAPID_NOT_SUPPORT_AGGREGATION:
        return "PICO_RAPID_NOT_SUPPORT_AGGREGATION: Aggregation was requested in rapid block mode.";
    case PICO_INVALID_TRIGGER_PROPERTY:
        return "PICO_INVALID_TRIGGER_PROPERTY: An invalid parameter was passed to SetTriggerChannelProperties.";
    case PICO_INTERFACE_NOT_CONNECTED:
        return "PICO_INTERFACE_NOT_CONNECTED: The driver was unable to contact the oscilloscope.";
    case PICO_RESISTANCE_AND_PROBE_NOT_ALLOWED:
        return "PICO_RESISTANCE_AND_PROBE_NOT_ALLOWED: Resistance-measuring mode is not allowed in conjunction with the specified probe.";
    case PICO_POWER_FAILED:
        return "PICO_POWER_FAILED: The device was unexpectedly powered down.";
    case PICO_SIGGEN_WAVEFORM_SETUP_FAILED:
        return "PICO_SIGGEN_WAVEFORM_SETUP_FAILED: A problem occurred in SetSigGenBuiltIn or SetSigGenArbitrary.";
    case PICO_FPGA_FAIL:
        return "PICO_FPGA_FAIL: FPGA not successfully set up.";
    case PICO_POWER_MANAGER:
        return "PICO_POWER_MANAGER: Power manager error.";
    case PICO_INVALID_ANALOGUE_OFFSET:
        return "PICO_INVALID_ANALOGUE_OFFSET: An impossible analog offset value was specified in SetChannel.";
    case PICO_PLL_LOCK_FAILED:
        return "PICO_PLL_LOCK_FAILED: There is an error within the device hardware.";
    case PICO_ANALOG_BOARD:
        return "PICO_ANALOG_BOARD: There is an error within the device hardware.";
    case PICO_CONFIG_FAIL_AWG:
        return "PICO_CONFIG_FAIL_AWG: Unable to configure the signal generator.";
    case PICO_INITIALISE_FPGA:
        return "PICO_INITIALISE_FPGA: The FPGA cannot be initialized, so unit cannot be opened.";
    case PICO_EXTERNAL_FREQUENCY_INVALID:
        return "PICO_EXTERNAL_FREQUENCY_INVALID: The frequency for the external clock is not within 15% of the nominal value.";
    case PICO_CLOCK_CHANGE_ERROR:
        return "PICO_CLOCK_CHANGE_ERROR: The FPGA could not lock the clock signal.";
    case PICO_TRIGGER_AND_EXTERNAL_CLOCK_CLASH:
        return "PICO_TRIGGER_AND_EXTERNAL_CLOCK_CLASH: You are trying to configure the AUX input as both a trigger and a reference clock.";
    case PICO_PWQ_AND_EXTERNAL_CLOCK_CLASH:
        return "PICO_PWQ_AND_EXTERNAL_CLOCK_CLASH: You are trying to configure the AUX input as both a pulse width qualifier and a reference clock.";
    case PICO_UNABLE_TO_OPEN_SCALING_FILE:
        return "PICO_UNABLE_TO_OPEN_SCALING_FILE: The requested scaling file cannot be opened.";
    case PICO_MEMORY_CLOCK_FREQUENCY:
        return "PICO_MEMORY_CLOCK_FREQUENCY: The frequency of the memory is reporting incorrectly.";
    case PICO_I2C_NOT_RESPONDING:
        return "PICO_I2C_NOT_RESPONDING: The I2C that is being actioned is not responding to requests.";
    case PICO_NO_CAPTURES_AVAILABLE:
        return "PICO_NO_CAPTURES_AVAILABLE: There are no captures available and therefore no data can be returned.";
    case PICO_NOT_USED_IN_THIS_CAPTURE_MODE:
        return "PICO_NOT_USED_IN_THIS_CAPTURE_MODE: The capture mode the device is currently running in does not support the current request.";
    case PICO_TOO_MANY_TRIGGER_CHANNELS_IN_USE:
        return "PICO_TOO_MANY_TRIGGER_CHANNELS_IN_USE: The number of trigger channels is greater than 4.";
    case PICO_INVALID_TRIGGER_DIRECTION:
        return "PICO_INVALID_TRIGGER_DIRECTION: An invalid trigger direction has been specified.";
    case PICO_INVALID_TRIGGER_STATES:
        return "PICO_INVALID_TRIGGER_STATES: When more than 4 trigger channels are set and their trigger condition states are not CONDITION_TRUE.";

    // Network/IP Errors (0x00000103 - 0x0000010B)
    case PICO_GET_DATA_ACTIVE:
        return "PICO_GET_DATA_ACTIVE: Get data is active.";
    case PICO_IP_NETWORKED:
        return "PICO_IP_NETWORKED: The device is currently connected via the IP Network socket and thus the call made is not supported.";
    case PICO_INVALID_IP_ADDRESS:
        return "PICO_INVALID_IP_ADDRESS: An incorrect IP address has been passed to the driver.";
    case PICO_IPSOCKET_FAILED:
        return "PICO_IPSOCKET_FAILED: The IP socket has failed.";
    case PICO_IPSOCKET_TIMEDOUT:
        return "PICO_IPSOCKET_TIMEDOUT: The IP socket has timed out.";
    case PICO_SETTINGS_FAILED:
        return "PICO_SETTINGS_FAILED: Failed to apply the requested settings.";
    case PICO_NETWORK_FAILED:
        return "PICO_NETWORK_FAILED: The network connection has failed.";
    case PICO_WS2_32_DLL_NOT_LOADED:
        return "PICO_WS2_32_DLL_NOT_LOADED: Unable to load the WS2 DLL.";
    case PICO_INVALID_IP_PORT:
        return "PICO_INVALID_IP_PORT: The specified IP port is invalid.";
    case PICO_COUPLING_NOT_SUPPORTED:
        return "PICO_COUPLING_NOT_SUPPORTED: The type of coupling requested is not supported on the opened device.";
    case PICO_BANDWIDTH_NOT_SUPPORTED:
        return "PICO_BANDWIDTH_NOT_SUPPORTED: Bandwidth limiting is not supported on the opened device.";
    case PICO_INVALID_BANDWIDTH:
        return "PICO_INVALID_BANDWIDTH: The value requested for the bandwidth limit is out of range.";
    case PICO_AWG_NOT_SUPPORTED:
        return "PICO_AWG_NOT_SUPPORTED: The arbitrary waveform generator is not supported by the opened device.";
    case PICO_ETS_NOT_RUNNING:
        return "PICO_ETS_NOT_RUNNING: Data has been requested with ETS mode set but run block has not been called, or stop has been called.";
    case PICO_SIG_GEN_WHITENOISE_NOT_SUPPORTED:
        return "PICO_SIG_GEN_WHITENOISE_NOT_SUPPORTED: White noise output is not supported on the opened device.";
    case PICO_SIG_GEN_WAVETYPE_NOT_SUPPORTED:
        return "PICO_SIG_GEN_WAVETYPE_NOT_SUPPORTED: The wave type requested is not supported by the opened device.";
    case PICO_INVALID_DIGITAL_PORT:
        return "PICO_INVALID_DIGITAL_PORT: The requested digital port number is out of range (MSOs only).";
    case PICO_INVALID_DIGITAL_CHANNEL:
        return "PICO_INVALID_DIGITAL_CHANNEL: The digital channel is not in the range DIGITAL_CHANNEL0 to DIGITAL_CHANNEL15.";
    case PICO_INVALID_DIGITAL_TRIGGER_DIRECTION:
        return "PICO_INVALID_DIGITAL_TRIGGER_DIRECTION: The digital trigger direction is not a valid trigger direction.";
    case PICO_SIG_GEN_PRBS_NOT_SUPPORTED:
        return "PICO_SIG_GEN_PRBS_NOT_SUPPORTED: Signal generator does not generate pseudo-random binary sequence.";
    case PICO_ETS_NOT_AVAILABLE_WITH_LOGIC_CHANNELS:
        return "PICO_ETS_NOT_AVAILABLE_WITH_LOGIC_CHANNELS: When a digital port is enabled, ETS sample mode is not available for use.";
    case PICO_WARNING_REPEAT_VALUE:
        return "PICO_WARNING_REPEAT_VALUE: There has been no new sample taken, this value has already been returned previously.";
    case PICO_POWER_SUPPLY_CONNECTED:
        return "PICO_POWER_SUPPLY_CONNECTED: The DC power supply is connected.";
    case PICO_POWER_SUPPLY_NOT_CONNECTED:
        return "PICO_POWER_SUPPLY_NOT_CONNECTED: The DC power supply is not connected.";
    case PICO_POWER_SUPPLY_REQUEST_INVALID:
        return "PICO_POWER_SUPPLY_REQUEST_INVALID: Incorrect power mode passed for current power source.";
    case PICO_POWER_SUPPLY_UNDERVOLTAGE:
        return "PICO_POWER_SUPPLY_UNDERVOLTAGE: The supply voltage from the USB source is too low.";
    case PICO_CAPTURING_DATA:
        return "PICO_CAPTURING_DATA: The oscilloscope is in the process of capturing data.";
    case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:
        return "PICO_USB3_0_DEVICE_NON_USB3_0_PORT: A USB 3.0 device is connected to a non-USB 3.0 port.";
    case PICO_NOT_SUPPORTED_BY_THIS_DEVICE:
        return "PICO_NOT_SUPPORTED_BY_THIS_DEVICE: A function has been called that is not supported by the current device.";
    case PICO_INVALID_DEVICE_RESOLUTION:
        return "PICO_INVALID_DEVICE_RESOLUTION: The device resolution is invalid (out of range).";
    case PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION:
        return "PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION: The number of channels that can be enabled is limited in 15 and 16-bit modes.";
    case PICO_CHANNEL_DISABLED_DUE_TO_USB_POWERED:
        return "PICO_CHANNEL_DISABLED_DUE_TO_USB_POWERED: USB power not sufficient for all requested channels.";
    case PICO_SIGGEN_DC_VOLTAGE_NOT_CONFIGURABLE:
        return "PICO_SIGGEN_DC_VOLTAGE_NOT_CONFIGURABLE: The signal generator does not have a configurable DC offset.";
    case PICO_NO_TRIGGER_ENABLED_FOR_TRIGGER_IN_PRE_TRIG:
        return "PICO_NO_TRIGGER_ENABLED_FOR_TRIGGER_IN_PRE_TRIG: An attempt has been made to define pre-trigger delay without first enabling a trigger.";
    case PICO_TRIGGER_WITHIN_PRE_TRIG_NOT_ARMED:
        return "PICO_TRIGGER_WITHIN_PRE_TRIG_NOT_ARMED: An attempt has been made to define pre-trigger delay without first arming a trigger.";
    case PICO_TRIGGER_WITHIN_PRE_NOT_ALLOWED_WITH_DELAY:
        return "PICO_TRIGGER_WITHIN_PRE_NOT_ALLOWED_WITH_DELAY: Pre-trigger delay and post-trigger delay cannot be used at the same time.";
    case PICO_TRIGGER_INDEX_UNAVAILABLE:
        return "PICO_TRIGGER_INDEX_UNAVAILABLE: The array index points to a nonexistent trigger.";
    case PICO_AWG_CLOCK_FREQUENCY:
        return "PICO_AWG_CLOCK_FREQUENCY: AWG clock frequency error.";
    case PICO_TOO_MANY_CHANNELS_IN_USE:
        return "PICO_TOO_MANY_CHANNELS_IN_USE: There are more than 4 analog channels with a trigger condition set.";
    case PICO_NULL_CONDITIONS:
        return "PICO_NULL_CONDITIONS: The condition parameter is a null pointer.";
    case PICO_DUPLICATE_CONDITION_SOURCE:
        return "PICO_DUPLICATE_CONDITION_SOURCE: There is more than one condition pertaining to the same channel.";
    case PICO_INVALID_CONDITION_INFO:
        return "PICO_INVALID_CONDITION_INFO: The parameter relating to condition information is out of range.";
    case PICO_SETTINGS_READ_FAILED:
        return "PICO_SETTINGS_READ_FAILED: Reading the meta data has failed.";
    case PICO_SETTINGS_WRITE_FAILED:
        return "PICO_SETTINGS_WRITE_FAILED: Writing the meta data has failed.";
    case PICO_ARGUMENT_OUT_OF_RANGE:
        return "PICO_ARGUMENT_OUT_OF_RANGE: A parameter has a value out of the expected range.";
    case PICO_HARDWARE_VERSION_NOT_SUPPORTED:
        return "PICO_HARDWARE_VERSION_NOT_SUPPORTED: The driver does not support the hardware variant connected.";
    case PICO_DIGITAL_HARDWARE_VERSION_NOT_SUPPORTED:
        return "PICO_DIGITAL_HARDWARE_VERSION_NOT_SUPPORTED: The driver does not support the digital hardware variant connected.";
    case PICO_ANALOGUE_HARDWARE_VERSION_NOT_SUPPORTED:
        return "PICO_ANALOGUE_HARDWARE_VERSION_NOT_SUPPORTED: The driver does not support the analog hardware variant connected.";
    case PICO_UNABLE_TO_CONVERT_TO_RESISTANCE:
        return "PICO_UNABLE_TO_CONVERT_TO_RESISTANCE: Converting a channel's ADC value to resistance has failed.";
    case PICO_DUPLICATED_CHANNEL:
        return "PICO_DUPLICATED_CHANNEL: The channel is listed more than once in the function call.";
    case PICO_INVALID_RESISTANCE_CONVERSION:
        return "PICO_INVALID_RESISTANCE_CONVERSION: The range cannot have resistance conversion applied.";
    case PICO_INVALID_VALUE_IN_MAX_BUFFER:
        return "PICO_INVALID_VALUE_IN_MAX_BUFFER: An invalid value is in the max buffer.";
    case PICO_INVALID_VALUE_IN_MIN_BUFFER:
        return "PICO_INVALID_VALUE_IN_MIN_BUFFER: An invalid value is in the min buffer.";
    case PICO_SIGGEN_FREQUENCY_OUT_OF_RANGE:
        return "PICO_SIGGEN_FREQUENCY_OUT_OF_RANGE: When calculating the frequency for phase conversion, the frequency is greater than that supported by the current variant.";
    case PICO_EEPROM2_CORRUPT:
        return "PICO_EEPROM2_CORRUPT: The device's EEPROM is corrupt. Contact Pico Technology support.";
    case PICO_EEPROM2_FAIL:
        return "PICO_EEPROM2_FAIL: The EEPROM has failed.";
    case PICO_SERIAL_BUFFER_TOO_SMALL:
        return "PICO_SERIAL_BUFFER_TOO_SMALL: The serial buffer is too small for the required information.";
    case PICO_SIGGEN_TRIGGER_AND_EXTERNAL_CLOCK_CLASH:
        return "PICO_SIGGEN_TRIGGER_AND_EXTERNAL_CLOCK_CLASH: The signal generator trigger and the external clock have both been set. This is not allowed.";
    case PICO_WARNING_SIGGEN_AUXIO_TRIGGER_DISABLED:
        return "PICO_WARNING_SIGGEN_AUXIO_TRIGGER_DISABLED: The AUX trigger was enabled and the external clock has been enabled, so the AUX has been automatically disabled.";
    case PICO_SIGGEN_GATING_AUXIO_NOT_AVAILABLE:
        return "PICO_SIGGEN_GATING_AUXIO_NOT_AVAILABLE: The AUX I/O was set as a scope trigger and is now being set as a signal generator gating trigger. This is not allowed.";
    case PICO_SIGGEN_GATING_AUXIO_ENABLED:
        return "PICO_SIGGEN_GATING_AUXIO_ENABLED: The AUX I/O was set by the signal generator as a gating trigger and is now being set as a scope trigger. This is not allowed.";
    case PICO_RESOURCE_ERROR:
        return "PICO_RESOURCE_ERROR: A resource has failed to initialise.";
    case PICO_TEMPERATURE_TYPE_INVALID:
        return "PICO_TEMPERATURE_TYPE_INVALID: The temperature type is out of range.";
    case PICO_TEMPERATURE_TYPE_NOT_SUPPORTED:
        return "PICO_TEMPERATURE_TYPE_NOT_SUPPORTED: A requested temperature type is not supported on this device.";
    case PICO_TIMEOUT:
        return "PICO_TIMEOUT: A read/write to the device has timed out.";
    case PICO_DEVICE_NOT_FUNCTIONING:
        return "PICO_DEVICE_NOT_FUNCTIONING: The device cannot be connected correctly.";
    case PICO_INTERNAL_ERROR:
        return "PICO_INTERNAL_ERROR: The driver has experienced an unknown error and is unable to recover from this error.";
    case PICO_MULTIPLE_DEVICES_FOUND:
        return "PICO_MULTIPLE_DEVICES_FOUND: Used when opening units via IP and more than multiple units have the same IP address.";
    case PICO_WARNING_NUMBER_OF_SEGMENTS_REDUCED:
        return "PICO_WARNING_NUMBER_OF_SEGMENTS_REDUCED: The number of segments has been reduced.";
    case PICO_CAL_PINS_STATES:
        return "PICO_CAL_PINS_STATES: The calibration pin states argument is out of range.";
    case PICO_CAL_PINS_FREQUENCY:
        return "PICO_CAL_PINS_FREQUENCY: The calibration pin frequency argument is out of range.";
    case PICO_CAL_PINS_AMPLITUDE:
        return "PICO_CAL_PINS_AMPLITUDE: The calibration pin amplitude argument is out of range.";
    case PICO_CAL_PINS_WAVETYPE:
        return "PICO_CAL_PINS_WAVETYPE: The calibration pin wavetype argument is out of range.";
    case PICO_CAL_PINS_OFFSET:
        return "PICO_CAL_PINS_OFFSET: The calibration pin offset argument is out of range.";
    case PICO_PROBE_FAULT:
        return "PICO_PROBE_FAULT: The probe's identity has a problem.";
    case PICO_PROBE_IDENTITY_UNKNOWN:
        return "PICO_PROBE_IDENTITY_UNKNOWN: The probe has not been identified.";
    case PICO_PROBE_POWER_DC_POWER_SUPPLY_REQUIRED:
        return "PICO_PROBE_POWER_DC_POWER_SUPPLY_REQUIRED: Enabling the probe would cause the device to exceed the allowable current limit.";
    case PICO_PROBE_NOT_POWERED_WITH_DC_POWER_SUPPLY:
        return "PICO_PROBE_NOT_POWERED_WITH_DC_POWER_SUPPLY: The DC power supply is connected; enabling the probe would cause the device to exceed the allowable current limit.";
    case PICO_PROBE_CONFIG_FAILURE:
        return "PICO_PROBE_CONFIG_FAILURE: Failed to complete probe configuration.";
    case PICO_PROBE_INTERACTION_CALLBACK:
        return "PICO_PROBE_INTERACTION_CALLBACK: Failed to set the callback function, as currently in current callback function.";
    case PICO_UNKNOWN_INTELLIGENT_PROBE:
        return "PICO_UNKNOWN_INTELLIGENT_PROBE: The probe has been verified but not known on this driver.";
    case PICO_INTELLIGENT_PROBE_CORRUPT:
        return "PICO_INTELLIGENT_PROBE_CORRUPT: The intelligent probe cannot be verified.";
    case PICO_PROBE_COLLECTION_NOT_STARTED:
        return "PICO_PROBE_COLLECTION_NOT_STARTED: The callback is null, probe collection will only start when first callback is a none null pointer.";
    case PICO_PROBE_POWER_CONSUMPTION_EXCEEDED:
        return "PICO_PROBE_POWER_CONSUMPTION_EXCEEDED: The current drawn by the probe(s) has exceeded the allowed limit.";
    case PICO_WARNING_PROBE_CHANNEL_OUT_OF_SYNC:
        return "PICO_WARNING_PROBE_CHANNEL_OUT_OF_SYNC: The channel range limits have changed due to connecting or disconnecting a probe the channel has been enabled.";
    case PICO_ENDPOINT_MISSING:
        return "PICO_ENDPOINT_MISSING: Endpoint missing.";
    case PICO_UNKNOWN_ENDPOINT_REQUEST:
        return "PICO_UNKNOWN_ENDPOINT_REQUEST: Unknown endpoint request.";
    case PICO_ADC_TYPE_ERROR:
        return "PICO_ADC_TYPE_ERROR: The ADC on board the device has not been correctly identified.";
    case PICO_FPGA2_FAILED:
        return "PICO_FPGA2_FAILED: FPGA2 failed.";
    case PICO_FPGA2_DEVICE_STATUS:
        return "PICO_FPGA2_DEVICE_STATUS: FPGA2 device status error.";
    case PICO_ENABLE_PROGRAM_FPGA2_FAILED:
        return "PICO_ENABLE_PROGRAM_FPGA2_FAILED: Enable program FPGA2 failed.";
    case PICO_NO_CHANNELS_OR_PORTS_ENABLED:
        return "PICO_NO_CHANNELS_OR_PORTS_ENABLED: No channels or ports enabled.";
    case PICO_INVALID_RATIO_MODE:
        return "PICO_INVALID_RATIO_MODE: Invalid ratio mode.";
    case PICO_READS_NOT_SUPPORTED_IN_CURRENT_CAPTURE_MODE:
        return "PICO_READS_NOT_SUPPORTED_IN_CURRENT_CAPTURE_MODE: Reads not supported in current capture mode.";
    case PICO_TRIGGER_READ_SELECTION_CHECK_FAILED:
        return "PICO_TRIGGER_READ_SELECTION_CHECK_FAILED: Trigger read selection check failed.";
    case PICO_DATA_READ1_SELECTION_CHECK_FAILED:
        return "PICO_DATA_READ1_SELECTION_CHECK_FAILED: Data read 1 selection check failed.";
    case PICO_DATA_READ2_SELECTION_CHECK_FAILED:
        return "PICO_DATA_READ2_SELECTION_CHECK_FAILED: Data read 2 selection check failed.";
    case PICO_DATA_READ3_SELECTION_CHECK_FAILED:
        return "PICO_DATA_READ3_SELECTION_CHECK_FAILED: Data read 3 selection check failed.";
    case PICO_READ_SELECTION_OUT_OF_RANGE:
        return "PICO_READ_SELECTION_OUT_OF_RANGE: The requested read is not one of the reads available.";
    case PICO_MULTIPLE_RATIO_MODES:
        return "PICO_MULTIPLE_RATIO_MODES: The downsample ratio options cannot be combined together for this request.";
    case PICO_NO_SAMPLES_READ:
        return "PICO_NO_SAMPLES_READ: The enPicoReadSelection request has no samples available.";
    case PICO_RATIO_MODE_NOT_REQUESTED:
        return "PICO_RATIO_MODE_NOT_REQUESTED: The enPicoReadSelection did not include one of the downsample ratios now requested.";
    case PICO_NO_USER_READ_REQUESTS_SET:
        return "PICO_NO_USER_READ_REQUESTS_SET: No read requests have been made.";
    case PICO_ZERO_SAMPLES_INVALID:
        return "PICO_ZERO_SAMPLES_INVALID: The parameter for number of values cannot be zero.";
    case PICO_ANALOGUE_HARDWARE_MISSING:
        return "PICO_ANALOGUE_HARDWARE_MISSING: The analog hardware cannot be identified; contact Pico Technology Technical Support.";
    case PICO_ANALOGUE_HARDWARE_PINS:
        return "PICO_ANALOGUE_HARDWARE_PINS: Setting of the analog hardware pins failed.";
    case PICO_ANALOGUE_HARDWARE_SMPS_FAULT:
        return "PICO_ANALOGUE_HARDWARE_SMPS_FAULT: An SMPS fault has occurred.";
    case PICO_DIGITAL_ANALOGUE_HARDWARE_CONFLICT:
        return "PICO_DIGITAL_ANALOGUE_HARDWARE_CONFLICT: There appears to be a conflict between the expected and actual hardware in the device.";
    case PICO_RATIO_MODE_BUFFER_NOT_SET:
        return "PICO_RATIO_MODE_BUFFER_NOT_SET: One or more of the enPicoRatioMode requested do not have a data buffer set.";
    case PICO_RESOLUTION_NOT_SUPPORTED_BY_VARIANT:
        return "PICO_RESOLUTION_NOT_SUPPORTED_BY_VARIANT: The resolution is valid but not supported by the opened device.";
    case PICO_THRESHOLD_OUT_OF_RANGE:
        return "PICO_THRESHOLD_OUT_OF_RANGE: The requested trigger threshold is out of range for the current device resolution.";
    case PICO_INVALID_SIMPLE_TRIGGER_DIRECTION:
        return "PICO_INVALID_SIMPLE_TRIGGER_DIRECTION: The simple trigger only supports upper edge direction options.";
    case PICO_AUX_NOT_SUPPORTED:
        return "PICO_AUX_NOT_SUPPORTED: The aux trigger is not supported on this variant.";
    case PICO_NULL_DIRECTIONS:
        return "PICO_NULL_DIRECTIONS: The trigger directions pointer may not be null.";
    case PICO_NULL_CHANNEL_PROPERTIES:
        return "PICO_NULL_CHANNEL_PROPERTIES: The trigger channel properties pointer may not be null.";
    case PICO_TRIGGER_CHANNEL_NOT_ENABLED:
        return "PICO_TRIGGER_CHANNEL_NOT_ENABLED: A trigger is set on a channel that has not been enabled.";
    case PICO_CONDITION_HAS_NO_TRIGGER_PROPERTY:
        return "PICO_CONDITION_HAS_NO_TRIGGER_PROPERTY: A trigger condition has been set but a trigger property not set.";
    case PICO_RATIO_MODE_TRIGGER_MASKING_INVALID:
        return "PICO_RATIO_MODE_TRIGGER_MASKING_INVALID: When requesting trigger data, this option can only be combined with the segment header ratio mode flag.";
    case PICO_TRIGGER_DATA_REQUIRES_MIN_BUFFER_SIZE_OF_40_SAMPLES:
        return "PICO_TRIGGER_DATA_REQUIRES_MIN_BUFFER_SIZE_OF_40_SAMPLES: The trigger data buffer must be 40 or more samples in size.";
    case PICO_NO_OF_CAPTURES_OUT_OF_RANGE:
        return "PICO_NO_OF_CAPTURES_OUT_OF_RANGE: The number of requested waveforms is greater than the number of memory segments allocated.";
    case PICO_RATIO_MODE_SEGMENT_HEADER_DOES_NOT_REQUIRE_BUFFERS:
        return "PICO_RATIO_MODE_SEGMENT_HEADER_DOES_NOT_REQUIRE_BUFFERS: When requesting segment header information, the segment header does not require a data buffer.";
    case PICO_FOR_SEGMENT_HEADER_USE_GETTRIGGERINFO:
        return "PICO_FOR_SEGMENT_HEADER_USE_GETTRIGGERINFO: Use GetTriggerInfo to retrieve the segment header information.";
    case PICO_READ_NOT_SET:
        return "PICO_READ_NOT_SET: A read request has not been set.";
    case PICO_ADC_SETTING_MISMATCH:
        return "PICO_ADC_SETTING_MISMATCH: The expected and actual states of the ADCs do not match.";
    case PICO_DATATYPE_INVALID:
        return "PICO_DATATYPE_INVALID: The requested data type is not one of the enPicoDataType listed.";
    case PICO_RATIO_MODE_DOES_NOT_SUPPORT_DATATYPE:
        return "PICO_RATIO_MODE_DOES_NOT_SUPPORT_DATATYPE: The down sample ratio mode requested does not support the enPicoDataType option chosen.";
    case PICO_CHANNEL_COMBINATION_NOT_VALID_IN_THIS_RESOLUTION:
        return "PICO_CHANNEL_COMBINATION_NOT_VALID_IN_THIS_RESOLUTION: The channel combination is not valid for the resolution.";
    case PICO_USE_8BIT_RESOLUTION:
        return "PICO_USE_8BIT_RESOLUTION: Use 8-bit resolution.";
    case PICO_AGGREGATE_BUFFERS_SAME_POINTER:
        return "PICO_AGGREGATE_BUFFERS_SAME_POINTER: The buffer for minimum data values and maximum data values are the same buffers.";
    case PICO_OVERLAPPED_READ_VALUES_OUT_OF_RANGE:
        return "PICO_OVERLAPPED_READ_VALUES_OUT_OF_RANGE: The read request number of samples requested for an overlapped operation are more than the total number of samples to capture.";
    case PICO_OVERLAPPED_READ_SEGMENTS_OUT_OF_RANGE:
        return "PICO_OVERLAPPED_READ_SEGMENTS_OUT_OF_RANGE: The overlapped read request has more segments specified than segments allocated.";
    case PICO_CHANNELFLAGSCOMBINATIONS_ARRAY_SIZE_TOO_SMALL:
        return "PICO_CHANNELFLAGSCOMBINATIONS_ARRAY_SIZE_TOO_SMALL: The number of channel combinations available are greater than the array size received.";
    case PICO_CAPTURES_EXCEEDS_NO_OF_SUPPORTED_SEGMENTS:
        return "PICO_CAPTURES_EXCEEDS_NO_OF_SUPPORTED_SEGMENTS: The number of captures is larger than the maximum number of segments allowed for the device variant.";
    case PICO_TIME_UNITS_OUT_OF_RANGE:
        return "PICO_TIME_UNITS_OUT_OF_RANGE: The time unit requested is not one of the listed enPicoTimeUnits.";
    case PICO_NO_SAMPLES_REQUESTED:
        return "PICO_NO_SAMPLES_REQUESTED: The number of samples parameter may not be zero.";
    case PICO_INVALID_ACTION:
        return "PICO_INVALID_ACTION: The action requested is not listed in enPicoAction.";
    case PICO_NO_OF_SAMPLES_NEED_TO_BE_EQUAL_WHEN_ADDING_BUFFERS:
        return "PICO_NO_OF_SAMPLES_NEED_TO_BE_EQUAL_WHEN_ADDING_BUFFERS: When adding buffers for the same read request the buffers for all ratio mode requests have to be the same size.";
    case PICO_WAITING_FOR_DATA_BUFFERS:
        return "PICO_WAITING_FOR_DATA_BUFFERS: The data is being processed but there is no empty data buffers available.";
    case PICO_STREAMING_ONLY_SUPPORTS_ONE_READ:
        return "PICO_STREAMING_ONLY_SUPPORTS_ONE_READ: When streaming data, only one read option is available.";
    case PICO_CLEAR_DATA_BUFFER_INVALID:
        return "PICO_CLEAR_DATA_BUFFER_INVALID: A clear read request is not one of the enPicoAction listed.";
    case PICO_INVALID_ACTION_FLAGS_COMBINATION:
        return "PICO_INVALID_ACTION_FLAGS_COMBINATION: The combination of action flags are not allowed.";
    case PICO_BOTH_MIN_AND_MAX_NULL_BUFFERS_CANNOT_BE_ADDED:
        return "PICO_BOTH_MIN_AND_MAX_NULL_BUFFERS_CANNOT_BE_ADDED: PICO_ADD request has been made but both data buffers are set to null.";
    case PICO_CONFLICT_IN_SET_DATA_BUFFERS_CALL_REMOVE_DATA_BUFFER_TO_RESET:
        return "PICO_CONFLICT_IN_SET_DATA_BUFFERS_CALL_REMOVE_DATA_BUFFER_TO_RESET: A conflict between the data buffers being set has occurred. Please use the PICO_CLEAR_ALL action to reset.";
    case PICO_REMOVING_DATA_BUFFER_ENTRIES_NOT_ALLOWED_WHILE_DATA_PROCESSING:
        return "PICO_REMOVING_DATA_BUFFER_ENTRIES_NOT_ALLOWED_WHILE_DATA_PROCESSING: While processing data, buffers cannot be removed from the data buffers list.";
    case PICO_CYUSB_REQUEST_FAILED:
        return "PICO_CYUSB_REQUEST_FAILED: An USB request has failed.";
    case PICO_STREAMING_DATA_REQUIRED:
        return "PICO_STREAMING_DATA_REQUIRED: A request has been made to retrieve the latest streaming data, but with either a null pointer or an array size set to zero.";
    case PICO_INVALID_NUMBER_OF_SAMPLES:
        return "PICO_INVALID_NUMBER_OF_SAMPLES: A buffer being set has a length that is invalid (ie less than zero).";
    case PICO_INVALID_DISTRIBUTION:
        return "PICO_INVALID_DISTRIBUTION: The distribution size may not be zero.";
    case PICO_BUFFER_LENGTH_GREATER_THAN_INT32_T:
        return "PICO_BUFFER_LENGTH_GREATER_THAN_INT32_T: The buffer length in bytes is greater than a 4-byte word.";
    case PICO_PLL_MUX_OUT_FAILED:
        return "PICO_PLL_MUX_OUT_FAILED: The PLL has failed.";
    case PICO_ONE_PULSE_WIDTH_DIRECTION_ALLOWED:
        return "PICO_ONE_PULSE_WIDTH_DIRECTION_ALLOWED: Pulse width only supports one direction.";
    case PICO_EXTERNAL_TRIGGER_NOT_SUPPORTED:
        return "PICO_EXTERNAL_TRIGGER_NOT_SUPPORTED: There is no external trigger available on the device specified by the handle.";
    case PICO_NO_TRIGGER_CONDITIONS_SET:
        return "PICO_NO_TRIGGER_CONDITIONS_SET: The condition parameter is a null pointer.";
    case PICO_NO_OF_CHANNEL_TRIGGER_PROPERTIES_OUT_OF_RANGE:
        return "PICO_NO_OF_CHANNEL_TRIGGER_PROPERTIES_OUT_OF_RANGE: The number of trigger channel properties it outside the allowed range (is less than zero).";
    case PICO_PROBE_COMPONENT_ERROR:
        return "PICO_PROBE_COMPONENT_ERROR: A probe has been plugged into a channel, but can not be identified correctly.";
    case PICO_INCOMPATIBLE_PROBE:
        return "PICO_INCOMPATIBLE_PROBE: The probe is incompatible with the device channel it is connected to.";
    case PICO_INVALID_TRIGGER_CHANNEL_FOR_ETS:
        return "PICO_INVALID_TRIGGER_CHANNEL_FOR_ETS: The requested channel for ETS triggering is not supported.";
    case PICO_NOT_AVAILABLE_WHEN_STREAMING_IS_RUNNING:
        return "PICO_NOT_AVAILABLE_WHEN_STREAMING_IS_RUNNING: While the device is streaming the get values method is not available.";
    case PICO_INVALID_TRIGGER_WITHIN_PRE_TRIGGER_STATE:
        return "PICO_INVALID_TRIGGER_WITHIN_PRE_TRIGGER_STATE: The requested state is not one of the enSharedTriggerWithinPreTrigger values.";
    case PICO_ZERO_NUMBER_OF_CAPTURES_INVALID:
        return "PICO_ZERO_NUMBER_OF_CAPTURES_INVALID: The number of captures have to be greater than zero.";
    case PICO_INVALID_LENGTH:
        return "PICO_INVALID_LENGTH: The quantifier for a pointer, defining the length in bytes is invalid.";
    case PICO_TRIGGER_DELAY_OUT_OF_RANGE:
        return "PICO_TRIGGER_DELAY_OUT_OF_RANGE: The trigger delay is greater than supported by the hardware.";
    case PICO_INVALID_THRESHOLD_DIRECTION:
        return "PICO_INVALID_THRESHOLD_DIRECTION: The requested threshold direction is not allowed with the specified channel.";
    case PICO_INVALID_THRESHOLD_MODE:
        return "PICO_INVALID_THRESHOLD_MODE: The requested threshold mode is not allowed with the specified channel.";
    case PICO_TIMEBASE_NOT_SUPPORTED_BY_RESOLUTION:
        return "PICO_TIMEBASE_NOT_SUPPORTED_BY_RESOLUTION: The timebase is not supported or is invalid.";

    // Variant/Memory Errors (0x00001000 - 0x00001001)
    case PICO_INVALID_VARIANT:
        return "PICO_INVALID_VARIANT: The device variant is not supported by this current driver.";
    case PICO_MEMORY_MODULE_ERROR:
        return "PICO_MEMORY_MODULE_ERROR: The actual memory module does not match the expected memory module.";

    // Pulse Width Qualifier Errors (0x00002000 - 0x00002007)
    case PICO_PULSE_WIDTH_QUALIFIER_LOWER_UPPER_CONFILCT:
        return "PICO_PULSE_WIDTH_QUALIFIER_LOWER_UPPER_CONFILCT: A null pointer has been passed in the trigger function or one of the parameters is out of range.";
    case PICO_PULSE_WIDTH_QUALIFIER_TYPE:
        return "PICO_PULSE_WIDTH_QUALIFIER_TYPE: The pulse width qualifier type is not one of the listed options.";
    case PICO_PULSE_WIDTH_QUALIFIER_DIRECTION:
        return "PICO_PULSE_WIDTH_QUALIFIER_DIRECTION: The pulse width qualifier direction is not one of the listed options.";
    case PICO_THRESHOLD_MODE_OUT_OF_RANGE:
        return "PICO_THRESHOLD_MODE_OUT_OF_RANGE: The threshold range is not one of the listed options.";
    case PICO_TRIGGER_AND_PULSEWIDTH_DIRECTION_IN_CONFLICT:
        return "PICO_TRIGGER_AND_PULSEWIDTH_DIRECTION_IN_CONFLICT: The trigger direction and pulse width option conflict with each other.";
    case PICO_THRESHOLD_UPPER_LOWER_MISMATCH:
        return "PICO_THRESHOLD_UPPER_LOWER_MISMATCH: The thresholds upper limits and thresholds lower limits conflict with each other.";
    case PICO_PULSE_WIDTH_LOWER_OUT_OF_RANGE:
        return "PICO_PULSE_WIDTH_LOWER_OUT_OF_RANGE: The pulse width lower count is out of range.";
    case PICO_PULSE_WIDTH_UPPER_OUT_OF_RANGE:
        return "PICO_PULSE_WIDTH_UPPER_OUT_OF_RANGE: The pulse width upper count is out of range.";

    // Front Panel/I2C Device Errors (0x00002008 - 0x00002019)
    case PICO_FRONT_PANEL_ERROR:
        return "PICO_FRONT_PANEL_ERROR: The devices front panel has caused an error.";
    case PICO_FRONT_PANEL_MODE:
        return "PICO_FRONT_PANEL_MODE: The actual and expected mode of the front panel do not match.";
    case PICO_FRONT_PANEL_FEATURE:
        return "PICO_FRONT_PANEL_FEATURE: A front panel feature is not available or failed to configure.";
    case PICO_NO_PULSE_WIDTH_CONDITIONS_SET:
        return "PICO_NO_PULSE_WIDTH_CONDITIONS_SET: When setting the pulse width conditions either the pointer is null or the number of conditions is set to zero.";
    case PICO_TRIGGER_PORT_NOT_ENABLED:
        return "PICO_TRIGGER_PORT_NOT_ENABLED: A trigger condition exists for a port, but the port has not been enabled.";
    case PICO_DIGITAL_DIRECTION_NOT_SET:
        return "PICO_DIGITAL_DIRECTION_NOT_SET: A trigger condition exists for a port, but no digital channel directions have been set.";
    case PICO_I2C_DEVICE_INVALID_READ_COMMAND:
        return "PICO_I2C_DEVICE_INVALID_READ_COMMAND: Invalid I2C device read command.";
    case PICO_I2C_DEVICE_INVALID_RESPONSE:
        return "PICO_I2C_DEVICE_INVALID_RESPONSE: Invalid I2C device response.";
    case PICO_I2C_DEVICE_INVALID_WRITE_COMMAND:
        return "PICO_I2C_DEVICE_INVALID_WRITE_COMMAND: Invalid I2C device write command.";
    case PICO_I2C_DEVICE_ARGUMENT_OUT_OF_RANGE:
        return "PICO_I2C_DEVICE_ARGUMENT_OUT_OF_RANGE: I2C device argument out of range.";
    case PICO_I2C_DEVICE_MODE:
        return "PICO_I2C_DEVICE_MODE: The actual and expected mode do not match.";
    case PICO_I2C_DEVICE_SETUP_FAILED:
        return "PICO_I2C_DEVICE_SETUP_FAILED: While trying to configure the device, set up failed.";
    case PICO_I2C_DEVICE_FEATURE:
        return "PICO_I2C_DEVICE_FEATURE: A feature is not available or failed to configure.";
    case PICO_I2C_DEVICE_VALIDATION_FAILED:
        return "PICO_I2C_DEVICE_VALIDATION_FAILED: The device did not pass the validation checks.";
    case PICO_INTERNAL_HEADER_ERROR:
        return "PICO_INTERNAL_HEADER_ERROR: Internal header error.";
    case PICO_FAILED_TO_WRITE_HARDWARE_FAULT:
        return "PICO_FAILED_TO_WRITE_HARDWARE_FAULT: The device couldn't write the channel settings due to a hardware fault.";

    // MSO Errors (0x00003000 - 0x00003009)
    case PICO_MSO_TOO_MANY_EDGE_TRANSITIONS_WHEN_USING_PULSE_WIDTH:
        return "PICO_MSO_TOO_MANY_EDGE_TRANSITIONS_WHEN_USING_PULSE_WIDTH: The number of MSO's edge transitions being set is not supported by this device.";
    case PICO_INVALID_PROBE_LED_POSITION:
        return "PICO_INVALID_PROBE_LED_POSITION: A probe LED position requested is not one of the available probe positions.";
    case PICO_PROBE_LED_POSITION_NOT_SUPPORTED:
        return "PICO_PROBE_LED_POSITION_NOT_SUPPORTED: The LED position is not supported by the selected variant.";
    case PICO_DUPLICATE_PROBE_CHANNEL_LED_POSITION:
        return "PICO_DUPLICATE_PROBE_CHANNEL_LED_POSITION: A channel has more than one of the same LED position in the ProbeChannelLedSetting struct.";
    case PICO_PROBE_LED_FAILURE:
        return "PICO_PROBE_LED_FAILURE: Setting the probes LED has failed.";
    case PICO_PROBE_NOT_SUPPORTED_BY_THIS_DEVICE:
        return "PICO_PROBE_NOT_SUPPORTED_BY_THIS_DEVICE: Probe is not supported by the selected variant.";
    case PICO_INVALID_PROBE_NAME:
        return "PICO_INVALID_PROBE_NAME: The probe name is not in the list of enPicoConnectProbe enums.";
    case PICO_NO_PROBE_COLOUR_SETTINGS:
        return "PICO_NO_PROBE_COLOUR_SETTINGS: The number of colour settings are zero or a null pointer passed to the function.";
    case PICO_NO_PROBE_CONNECTED_ON_REQUESTED_CHANNEL:
        return "PICO_NO_PROBE_CONNECTED_ON_REQUESTED_CHANNEL: Channel has no probe connected to it.";
    case PICO_PROBE_DOES_NOT_REQUIRE_CALIBRATION:
        return "PICO_PROBE_DOES_NOT_REQUIRE_CALIBRATION: Connected probe does not require calibration.";
    case PICO_PROBE_CALIBRATION_FAILED:
        return "PICO_PROBE_CALIBRATION_FAILED: Connected probe could not be calibrated - hardware fault is a possible cause.";
    case PICO_PROBE_VERSION_ERROR:
        return "PICO_PROBE_VERSION_ERROR: A probe has been connected, but the version is not recognised.";
    case PICO_PROBE_DOES_NOT_SUPPORT_FREQUENCY_COUNTER:
        return "PICO_PROBE_DOES_NOT_SUPPORT_FREQUENCY_COUNTER: The channel with the frequency counter enabled has a probe connected that does not support this feature.";

    // Trigger Time Errors (0x00004000)
    case PICO_AUTO_TRIGGER_TIME_TOO_LONG:
        return "PICO_AUTO_TRIGGER_TIME_TOO_LONG: The requested trigger time is to long for the selected variant.";

    // MSO POD Errors (0x00005000 - 0x00005006)
    case PICO_MSO_POD_VALIDATION_FAILED:
        return "PICO_MSO_POD_VALIDATION_FAILED: The MSO pod did not pass the validation checks.";
    case PICO_NO_MSO_POD_CONNECTED:
        return "PICO_NO_MSO_POD_CONNECTED: No MSO pod found on the requested digital port.";
    case PICO_DIGITAL_PORT_HYSTERESIS_OUT_OF_RANGE:
        return "PICO_DIGITAL_PORT_HYSTERESIS_OUT_OF_RANGE: The digital port enum value is not in the enPicoDigitalPortHysteresis declaration.";
    case PICO_MSO_POD_FAILED_UNIT:
        return "PICO_MSO_POD_FAILED_UNIT: MSO pod failed unit.";
    case PICO_ATTENUATION_FAILED:
        return "PICO_ATTENUATION_FAILED: The device's EEPROM is corrupt. Contact Pico Technology support.";
    case PICO_DC_50OHM_OVERVOLTAGE_TRIPPED:
        return "PICO_DC_50OHM_OVERVOLTAGE_TRIPPED: A channel set to the 50Ohm Path has Tripped due to the input signal.";
    case PICO_MSO_OVER_CURRENT_TRIPPED:
        return "PICO_MSO_OVER_CURRENT_TRIPPED: The MSO pod over current protection activated, unplug and replug the MSO pod.";

    // Overheating Error (0x00005010)
    case PICO_NOT_RESPONDING_OVERHEATED:
        return "PICO_NOT_RESPONDING_OVERHEATED: The device has overheated.";

    // USB Version Error (0x00005100)
    case PICO_USB_VERSION_NOT_SUPPORTED:
        return "PICO_USB_VERSION_NOT_SUPPORTED: The USB version of the port is not supported by this variant.";

    // Hardware Capture Errors (0x00006000 - 0x00006002)
    case PICO_HARDWARE_CAPTURE_TIMEOUT:
        return "PICO_HARDWARE_CAPTURE_TIMEOUT: Waiting for the device to capture timed out.";
    case PICO_HARDWARE_READY_TIMEOUT:
        return "PICO_HARDWARE_READY_TIMEOUT: Waiting for the device be ready for capture timed out.";
    case PICO_HARDWARE_CAPTURING_CALL_STOP:
        return "PICO_HARDWARE_CAPTURING_CALL_STOP: The driver is performing a capture requested by RunStreaming or RunBlock. To interrupt this capture call Stop on the device first.";

    // Streaming Errors (0x00007000 - 0x00007002)
    case PICO_TOO_FEW_REQUESTED_STREAMING_SAMPLES:
        return "PICO_TOO_FEW_REQUESTED_STREAMING_SAMPLES: The number of samples is less than the minimum number allowed.";
    case PICO_STREAMING_REREAD_DATA_NOT_AVAILABLE:
        return "PICO_STREAMING_REREAD_DATA_NOT_AVAILABLE: A streaming capture has been made but re-reading";
    default:
        return "UNKNOWN: Unrecognized PICO_INFO code.";
    }
}
#endif
