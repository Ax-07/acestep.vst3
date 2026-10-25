#include "PluginBackendClient.h"

#include <cmath>

namespace acestep::vst3
{
namespace
{
constexpr int kNetworkTimeoutMs = 15000;

juce::String normalizeBaseUrl(juce::String baseUrl)
{
    baseUrl = baseUrl.trim();
    while (baseUrl.endsWithChar('/'))
    {
        baseUrl = baseUrl.dropLastCharacters(1);
    }
    return baseUrl;
}

juce::URL buildUrl(const juce::String& baseUrl, const juce::String& path)
{
    if (path.startsWithIgnoreCase("http://") || path.startsWithIgnoreCase("https://"))
    {
        return juce::URL(path);
    }

    const auto normalizedBaseUrl = normalizeBaseUrl(baseUrl);
    const auto normalizedPath = path.startsWithChar('/') ? path : "/" + path;
    return juce::URL(normalizedBaseUrl + normalizedPath);
}

bool readJsonResponse(const juce::URL& url,
                      const juce::String& httpMethod,
                      const juce::String& requestBody,
                      const juce::String& contentType,
                      juce::var& parsedResponse,
                      juce::String& errorMessage)
{
    int statusCode = 0;
    auto requestUrl = requestBody.isNotEmpty() ? url.withPOSTData(requestBody) : url;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(kNetworkTimeoutMs)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(4)
                       .withHttpRequestCmd(httpMethod)
                       .withExtraHeaders(contentType.isEmpty() ? juce::String()
                                                              : "Content-Type: " + contentType
                                                                    + "\r\n");

    auto stream = requestUrl.createInputStream(options);
    if (stream == nullptr)
    {
        errorMessage = "Could not connect to the ACE-Step backend.";
        return false;
    }

    const auto responseText = stream->readEntireStreamAsString();
    parsedResponse = juce::JSON::parse(responseText);
    if (parsedResponse.isVoid())
    {
        errorMessage = responseText.isEmpty() ? "Backend returned an empty response."
                                              : "Backend returned invalid JSON.";
        return false;
    }

    auto* rootObject = parsedResponse.getDynamicObject();
    if (rootObject == nullptr)
    {
        errorMessage = "Backend returned an unexpected response shape.";
        return false;
    }

    if (statusCode != 200)
    {
        errorMessage = rootObject->getProperty("error").toString();
        if (errorMessage.isEmpty())
        {
            errorMessage = "Backend request failed with HTTP " + juce::String(statusCode) + ".";
        }
        return false;
    }

    return true;
}

juce::DynamicObject* getWrappedDataObject(const juce::var& response)
{
    auto* rootObject = response.getDynamicObject();
    if (rootObject == nullptr)
    {
        return nullptr;
    }

    if (static_cast<int>(rootObject->getProperty("code")) != 200)
    {
        return nullptr;
    }

    return rootObject->getProperty("data").getDynamicObject();
}

juce::String buildResultLabel(const juce::DynamicObject& itemObject, int slotIndex)
{
    const auto prompt = itemObject.getProperty("prompt").toString().trim();
    const auto promptLabel = prompt.isEmpty() ? "Untitled prompt" : prompt.substring(0, 28);
    const auto metasVar = itemObject.getProperty("metas");
    const auto* metas = metasVar.getDynamicObject();
    const auto duration = metas != nullptr ? metas->getProperty("duration").toString() : juce::String();
    const auto keyScale = metas != nullptr ? metas->getProperty("keyscale").toString() : juce::String();
    juce::String suffix;
    if (duration.isNotEmpty())
    {
        suffix << duration << "s";
    }
    if (keyScale.isNotEmpty())
    {
        if (suffix.isNotEmpty())
        {
            suffix << ", ";
        }
        suffix << keyScale;
    }

    juce::String label = "Result " + juce::String(slotIndex + 1) + " - " + promptLabel;
    if (suffix.isNotEmpty())
    {
        label << " (" << suffix << ")";
    }
    return label;
}

juce::String chooseModelName(ModelPreset preset)
{
    switch (preset)
    {
        case ModelPreset::turbo:
            return "acestep-v15-turbo";
        case ModelPreset::standard:
        case ModelPreset::quality:
            break;
    }

    return {};
}

int chooseInferenceSteps(QualityMode qualityMode)
{
    switch (qualityMode)
    {
        case QualityMode::fast:
            return 6;
        case QualityMode::balanced:
            return 8;
        case QualityMode::high:
            return 12;
    }

    return 8;
}

bool modeNeedsPrompt(WorkflowMode mode)
{
    return mode == WorkflowMode::text || mode == WorkflowMode::reference
           || mode == WorkflowMode::coverRemix;
}

juce::String chooseFileExtension(const juce::String& remoteFileUrl)
{
    const auto decoded = juce::URL::removeEscapeChars(remoteFileUrl);
    const auto dotIndex = decoded.lastIndexOfChar('.');
    if (dotIndex < 0)
    {
        return ".wav";
    }

    const auto extension = decoded.substring(dotIndex);
    if (extension.length() > 8 || extension.containsAnyOf("/?&"))
    {
        return ".wav";
    }

    return extension;
}

// Extract a bar-aligned passage from the interior of a longer AI render.
// Keep the untouched original for further editing in the DAW.
bool writeBarLengthWav(const juce::File& sourceFile,
                       const juce::File& loopFile,
                       double durationSeconds,
                       int loopBars,
                       bool antiClickFades,
                       double& selectedStartSeconds,
                       double& selectedEndSeconds,
                       juce::String& errorMessage)
{
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0 || loopBars <= 0)
    {
        errorMessage = "Invalid target loop length.";
        return false;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(sourceFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0)
    {
        errorMessage = "Could not read the original audio for loop export.";
        return false;
    }

    const auto totalSamples =
        static_cast<juce::int64>(std::llround(durationSeconds * reader->sampleRate));
    if (reader->lengthInSamples + static_cast<juce::int64>(reader->sampleRate * 0.01)
        < totalSamples)
    {
        errorMessage = "ACE-Step returned audio shorter than the requested bar loop.";
        return false;
    }

    auto output = loopFile.createOutputStream();
    if (output == nullptr || !output->openedOk())
    {
        errorMessage = "Could not create the bar-length WAV.";
        return false;
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(output.get(), reader->sampleRate, reader->numChannels, 24, {}, 0));
    if (writer == nullptr)
    {
        errorMessage = "Could not initialise the bar-length WAV writer.";
        return false;
    }
    output.release();  // The writer now owns and closes the output stream.

    const auto secondsPerBar = durationSeconds / loopBars;
    const auto sourceSeconds = reader->lengthInSamples / reader->sampleRate;
    const auto fullBarsAvailable =
        static_cast<juce::int64>(std::floor(sourceSeconds / secondsPerBar + 1.0e-6));
    const auto startBar = juce::jmax<juce::int64>(
        0, (fullBarsAvailable - static_cast<juce::int64>(loopBars)) / 2);
    const auto maxStartSample =
        juce::jmax<juce::int64>(0, reader->lengthInSamples - totalSamples);
    const auto startSample = juce::jmin<juce::int64>(
        maxStartSample,
        static_cast<juce::int64>(
            std::llround(startBar * secondsPerBar * reader->sampleRate)));

    const auto fadeSamples = juce::jmax<juce::int64>(
        1, static_cast<juce::int64>(std::llround(reader->sampleRate * 0.005)));
    constexpr int kChunkSamples = 4096;
    juce::AudioBuffer<float> samples(static_cast<int>(reader->numChannels), kChunkSamples);

    for (juce::int64 offset = 0; offset < totalSamples; offset += kChunkSamples)
    {
        const auto chunk = static_cast<int>(juce::jmin<juce::int64>(
            kChunkSamples, totalSamples - offset));
        samples.clear();
        if (!reader->read(&samples, 0, chunk, startSample + offset, true, true))
        {
            errorMessage = "Could not read the generated WAV for loop export.";
            return false;
        }

        // No fades by default: keep drum attacks intact. The user may
        // explicitly opt into 5 ms boundary fades to suppress clicks.
        if (antiClickFades)
        {
            for (int sample = 0; sample < chunk; ++sample)
            {
                const auto absoluteSample = offset + sample;
                const auto fadeIn = static_cast<float>(juce::jmin<juce::int64>(
                    absoluteSample, fadeSamples)) / static_cast<float>(fadeSamples);
                const auto fadeOut = static_cast<float>(juce::jmin<juce::int64>(
                    totalSamples - absoluteSample - 1, fadeSamples))
                    / static_cast<float>(fadeSamples);
                const auto gain = juce::jmin(fadeIn, fadeOut);
                for (int channel = 0; channel < samples.getNumChannels(); ++channel)
                    samples.setSample(channel, sample, samples.getSample(channel, sample) * gain);
            }
        }

        if (!writer->writeFromAudioSampleBuffer(samples, 0, chunk))
        {
            errorMessage = "Could not write the bar-length WAV.";
            return false;
        }
    }

    writer.reset();  // Finalise WAV headers before Ableton reads the file.
    selectedStartSeconds = startSample / reader->sampleRate;
    selectedEndSeconds = (startSample + totalSamples) / reader->sampleRate;
    return true;
}

PluginLoRAStatusResult parseLoRAStatusResponse(const juce::var& response)
{
    PluginLoRAStatusResult result;
    auto* dataObject = getWrappedDataObject(response);
    if (dataObject == nullptr)
    {
        result.errorMessage = "Backend did not return LoRA status data.";
        return result;
    }

    result.succeeded = true;
    result.loaded = static_cast<bool>(dataObject->getProperty("lora_loaded"));
    result.useLora = static_cast<bool>(dataObject->getProperty("use_lora"));
    result.scale = static_cast<double>(dataObject->getProperty("lora_scale"));
    result.activeAdapter = dataObject->getProperty("active_adapter").toString();
    if (const auto adaptersVar = dataObject->getProperty("adapters"); adaptersVar.isArray())
    {
        for (const auto& item : *adaptersVar.getArray())
        {
            const auto adapter = item.toString();
            if (adapter.isNotEmpty())
            {
                result.adapters.addIfNotAlreadyThere(adapter);
            }
        }
    }
    return result;
}

PluginLoRAOperationResult buildLoRAOperationResult(const juce::String& baseUrl,
                                                   const juce::var& response,
                                                   const PluginBackendClient& client)
{
    PluginLoRAOperationResult result;
    auto* dataObject = getWrappedDataObject(response);
    if (dataObject == nullptr)
    {
        result.errorMessage = "Backend did not return a LoRA operation payload.";
        return result;
    }

    result.message = dataObject->getProperty("message").toString();
    result.status = client.getLoRAStatus(baseUrl);
    result.succeeded = result.status.succeeded;
    if (!result.succeeded && result.errorMessage.isEmpty())
    {
        result.errorMessage = result.status.errorMessage;
    }
    return result;
}
}  // namespace

PluginHealthCheckResult PluginBackendClient::checkHealth(const juce::String& baseUrl) const
{
    PluginHealthCheckResult result;
    juce::var response;
    juce::String errorMessage;
    if (!readJsonResponse(buildUrl(baseUrl, "/health"), "GET", {}, {}, response, errorMessage))
    {
        result.status = BackendStatus::offline;
        result.errorMessage = errorMessage;
        return result;
    }

    auto* rootObject = response.getDynamicObject();
    auto* dataObject = rootObject != nullptr ? rootObject->getProperty("data").getDynamicObject() : nullptr;
    const auto status = dataObject != nullptr ? dataObject->getProperty("status").toString() : juce::String();
    result.status = status == "ok" ? BackendStatus::ready : BackendStatus::degraded;
    if (result.status != BackendStatus::ready)
    {
        result.errorMessage = "Backend health check returned a degraded status.";
    }
    return result;
}

PluginGenerationStartResult PluginBackendClient::startGeneration(const PluginState& state) const
{
    PluginGenerationStartResult result;

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    auto prompt = modeNeedsPrompt(state.workflowMode) ? state.prompt : juce::String();
    if (state.generationLoopBars > 0 && prompt.isNotEmpty())
    {
        prompt += ", continuous steady groove throughout, no intro, no outro, "
                  "no ending, no breakdown, no fade-out";
    }
    payload->setProperty("prompt", prompt);
    payload->setProperty("lyrics", state.lyrics);
    payload->setProperty("task_type",
                         state.workflowMode == WorkflowMode::coverRemix ? "cover" : "text2music");
    // In bar mode render extra musical context: at least 30 seconds, with
    // four spare bars before and after the selected passage. Seconds mode
    // remains unchanged. ACE-Step accepts up to 600 seconds.
    auto renderSeconds = static_cast<double>(state.durationSeconds);
    if (state.generationLoopBars > 0 && state.generationLoopDurationSeconds > 0.0)
    {
        const auto secondsPerBar =
            state.generationLoopDurationSeconds / state.generationLoopBars;
        renderSeconds = juce::jmin(
            600.0, juce::jmax(30.0, state.generationLoopDurationSeconds + 8.0 * secondsPerBar));
    }
    payload->setProperty("audio_duration", renderSeconds);
    if (state.generationBpm > 0)
        payload->setProperty("bpm", state.generationBpm);
    if (state.generationTimeSignature.isNotEmpty())
        payload->setProperty("time_signature", state.generationTimeSignature);
    payload->setProperty("batch_size", 1);
    payload->setProperty("audio_format", "wav");
    payload->setProperty("thinking", false);
    payload->setProperty("use_random_seed", false);
    payload->setProperty("seed", state.seed);
    payload->setProperty("inference_steps", chooseInferenceSteps(state.qualityMode));
    payload->setProperty("guidance_scale", 7.0);

    if (const auto modelName = chooseModelName(state.modelPreset); modelName.isNotEmpty())
    {
        payload->setProperty("model", modelName);
    }

    switch (state.workflowMode)
    {
        case WorkflowMode::text:
            break;
        case WorkflowMode::reference:
            payload->setProperty("reference_audio_path", state.referenceAudioPath);
            break;
        case WorkflowMode::coverRemix:
            payload->setProperty("src_audio_path", state.sourceAudioPath);
            payload->setProperty("audio_cover_strength", state.audioCoverStrength);
            if (state.referenceAudioPath.isNotEmpty())
            {
                payload->setProperty("reference_audio_path", state.referenceAudioPath);
            }
            break;
        case WorkflowMode::customConditioning:
            payload->setProperty("audio_code_string", state.customConditioningCodes);
            payload->setProperty("audio_cover_strength", state.audioCoverStrength);
            break;
    }

    juce::var response;
    juce::String errorMessage;
    if (!readJsonResponse(buildUrl(state.backendBaseUrl, "/release_task"),
                          "POST",
                          juce::JSON::toString(juce::var(payload.get())),
                          "application/json",
                          response,
                          errorMessage))
    {
        result.errorMessage = errorMessage;
        return result;
    }

    auto* dataObject = getWrappedDataObject(response);
    if (dataObject == nullptr)
    {
        auto* rootObject = response.getDynamicObject();
        result.errorMessage = rootObject != nullptr ? rootObject->getProperty("error").toString()
                                                    : "Backend did not return a task id.";
        if (result.errorMessage.isEmpty())
        {
            result.errorMessage = "Backend did not return a task id.";
        }
        return result;
    }

    result.taskId = dataObject->getProperty("task_id").toString();
    result.succeeded = result.taskId.isNotEmpty();
    if (!result.succeeded)
    {
        result.errorMessage = "Backend accepted the request but did not return a task id.";
    }
    return result;
}

PluginGenerationPollResult PluginBackendClient::pollGeneration(const juce::String& baseUrl,
                                                               const juce::String& taskId) const
{
    PluginGenerationPollResult result;
    result.status = JobStatus::failed;

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    juce::Array<juce::var> taskIds;
    taskIds.add(taskId);
    payload->setProperty("task_id_list", juce::var(taskIds));

    juce::var response;
    juce::String errorMessage;
    if (!readJsonResponse(buildUrl(baseUrl, "/query_result"),
                          "POST",
                          juce::JSON::toString(juce::var(payload.get())),
                          "application/json",
                          response,
                          errorMessage))
    {
        result.errorMessage = errorMessage;
        return result;
    }

    auto* rootObject = response.getDynamicObject();
    if (rootObject == nullptr)
    {
        result.errorMessage = "Backend returned an invalid query_result payload.";
        return result;
    }

    const auto dataVar = rootObject->getProperty("data");
    if (!dataVar.isArray() || dataVar.size() <= 0)
    {
        result.status = JobStatus::queuedOrRunning;
        result.progressText = "Waiting for backend result...";
        return result;
    }

    auto* taskObject = dataVar[0].getDynamicObject();
    if (taskObject == nullptr)
    {
        result.errorMessage = "Backend returned an invalid task payload.";
        return result;
    }

    const auto statusCode = static_cast<int>(taskObject->getProperty("status"));
    result.progressText = taskObject->getProperty("progress_text").toString();

    if (statusCode == 0)
    {
        result.status = JobStatus::queuedOrRunning;
        if (result.progressText.isEmpty())
        {
            result.progressText = "Queued / Running";
        }
        return result;
    }

    const auto parsedResult = juce::JSON::parse(taskObject->getProperty("result").toString());
    if (!parsedResult.isArray() || parsedResult.size() <= 0)
    {
        result.status = statusCode == 1 ? JobStatus::succeeded : JobStatus::failed;
        result.errorMessage = statusCode == 1 ? "Task finished without any audio file." : "Task failed.";
        return result;
    }

    if (statusCode == 2)
    {
        result.status = JobStatus::failed;
        auto* failedItem = parsedResult[0].getDynamicObject();
        result.errorMessage = failedItem != nullptr ? failedItem->getProperty("error").toString()
                                                    : "Task failed.";
        if (result.errorMessage.isEmpty())
        {
            result.errorMessage = "Task failed.";
        }
        return result;
    }

    result.status = JobStatus::succeeded;
    for (int index = 0; index < juce::jmin(kResultSlotCount, parsedResult.size()); ++index)
    {
        auto* itemObject = parsedResult[index].getDynamicObject();
        if (itemObject == nullptr)
        {
            continue;
        }

        auto& slot = result.resultSlots[static_cast<size_t>(index)];
        slot.remoteFileUrl = itemObject->getProperty("url").toString();
        if (slot.remoteFileUrl.isEmpty())
        {
            slot.remoteFileUrl = itemObject->getProperty("file").toString();
        }
        slot.label = buildResultLabel(*itemObject, index);
    }

    return result;
}

PluginPreviewDownloadResult PluginBackendClient::downloadPreviewFile(const juce::String& baseUrl,
                                                                     const juce::String& remoteFileUrl,
                                                                     int slotIndex,
                                                                     double loopDurationSeconds,
                                                                     int loopBars,
                                                                     bool antiClickFades) const
{
    PluginPreviewDownloadResult result;
    result.slotIndex = slotIndex;
    if (remoteFileUrl.isEmpty())
    {
        result.errorMessage = "The backend did not return an audio file URL.";
        return result;
    }

    int statusCode = 0;
    auto stream = buildUrl(baseUrl, remoteFileUrl)
                      .createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                             .withConnectionTimeoutMs(kNetworkTimeoutMs)
                                             .withStatusCode(&statusCode)
                                             .withNumRedirectsToFollow(4)
                                             .withHttpRequestCmd("GET"));
    if (stream == nullptr || statusCode != 200)
    {
        result.errorMessage = "Could not download the generated preview audio.";
        return result;
    }

    // Keep generated audio beyond this DAW session. Users can override the
    // default Music/ACE-Step/Exports folder with ACESTEP_EXPORT_DIR.
    const auto configuredDirectory =
        juce::SystemStats::getEnvironmentVariable("ACESTEP_EXPORT_DIR", {}).trim();
    const auto exportsDirectory = configuredDirectory.isNotEmpty()
                                      ? juce::File(configuredDirectory)
                                      : juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                            .getChildFile("ACE-Step")
                                            .getChildFile("Exports");
    if (exportsDirectory.createDirectory().failed())
    {
        result.errorMessage = "Could not create the audio exports folder: "
                              + exportsDirectory.getFullPathName();
        return result;
    }

    const auto filePrefix = "acestep-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")
                            + "-take-" + juce::String(slotIndex + 1);
    auto previewFile = exportsDirectory.getNonexistentChildFile(filePrefix,
                                                                chooseFileExtension(remoteFileUrl),
                                                                false);
    {
        juce::FileOutputStream output(previewFile);
        if (!output.openedOk())
        {
            result.errorMessage = "Could not create the audio export file.";
            return result;
        }

        const auto bytesWritten = output.writeFromInputStream(*stream, -1);
        output.flush();
        if (bytesWritten <= 0 || output.getStatus().failed())
        {
            result.errorMessage = "Could not save the generated audio file.";
            return result;
        }
    }

    result.originalFilePath = previewFile.getFullPathName();

    // The original remains available for further production/editing. The
    // shortened, bar-exact derivative is the one previewed and dragged to Live.
    if (loopBars > 0 && loopDurationSeconds > 0.0)
    {
        const auto loopFile = exportsDirectory.getNonexistentChildFile(
            filePrefix + "-" + juce::String(loopBars) + "bars", ".wav", false);
        juce::String loopError;
        if (!writeBarLengthWav(previewFile, loopFile, loopDurationSeconds,
                               loopBars, antiClickFades, result.loopStartSeconds,
                               result.loopEndSeconds, loopError))
        {
            result.errorMessage = loopError + " Original saved at: "
                                  + previewFile.getFullPathName();
            return result;
        }
        previewFile = loopFile;
    }

    result.succeeded = true;
    result.localFilePath = previewFile.getFullPathName();
    result.displayName = previewFile.getFileName();
    return result;
}

PluginLoRAStatusResult PluginBackendClient::getLoRAStatus(const juce::String& baseUrl) const
{
    juce::var response;
    juce::String errorMessage;
    if (!readJsonResponse(buildUrl(baseUrl, "/v1/lora/status"),
                          "GET",
                          {},
                          {},
                          response,
                          errorMessage))
    {
        PluginLoRAStatusResult result;
        result.errorMessage = errorMessage;
        return result;
    }

    return parseLoRAStatusResponse(response);
}

PluginLoRAOperationResult PluginBackendClient::loadLoRA(const juce::String& baseUrl,
                                                        const juce::String& loraPath) const
{
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("lora_path", loraPath);

    juce::var response;
    juce::String errorMessage;
    if (!readJsonResponse(buildUrl(baseUrl, "/v1/lora/load"),
                          "POST",
                          juce::JSON::toString(juce::var(payload.get())),
                          "application/json",
                          response,
                          errorMessage))
    {
        PluginLoRAOperationResult result;
        result.errorMessage = errorMessage;
        return result;
    }

    return buildLoRAOperationResult(baseUrl, response, *this);
}

PluginLoRAOperationResult PluginBackendClient::unloadLoRA(const juce::String& baseUrl) const
{
    juce::var response;
    juce::String errorMessage;
    if (!readJsonResponse(buildUrl(baseUrl, "/v1/lora/unload"),
                          "POST",
                          "{}",
                          "application/json",
                          response,
                          errorMessage))
    {
        PluginLoRAOperationResult result;
        result.errorMessage = errorMessage;
        return result;
    }

    return buildLoRAOperationResult(baseUrl, response, *this);
}

PluginLoRAOperationResult PluginBackendClient::toggleLoRA(const juce::String& baseUrl,
                                                          bool useLora) const
{
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("use_lora", useLora);

    juce::var response;
    juce::String errorMessage;
    if (!readJsonResponse(buildUrl(baseUrl, "/v1/lora/toggle"),
                          "POST",
                          juce::JSON::toString(juce::var(payload.get())),
                          "application/json",
                          response,
                          errorMessage))
    {
        PluginLoRAOperationResult result;
        result.errorMessage = errorMessage;
        return result;
    }

    return buildLoRAOperationResult(baseUrl, response, *this);
}

PluginLoRAOperationResult PluginBackendClient::setLoRAScale(const juce::String& baseUrl,
                                                            double scale,
                                                            const juce::String& adapterName) const
{
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("scale", scale);
    if (adapterName.isNotEmpty())
    {
        payload->setProperty("adapter_name", adapterName);
    }

    juce::var response;
    juce::String errorMessage;
    if (!readJsonResponse(buildUrl(baseUrl, "/v1/lora/scale"),
                          "POST",
                          juce::JSON::toString(juce::var(payload.get())),
                          "application/json",
                          response,
                          errorMessage))
    {
        PluginLoRAOperationResult result;
        result.errorMessage = errorMessage;
        return result;
    }

    return buildLoRAOperationResult(baseUrl, response, *this);
}
}  // namespace acestep::vst3
