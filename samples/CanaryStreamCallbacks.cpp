/**
 * Kinesis Video Producer Continuous Retry Stream Callbacks
 */
#define LOG_CLASS "CanaryStreamCallbacks"
#include "CanaryStreamCallbacks.h"

STATUS createCanaryStreamCallbacks(Aws::CloudWatch::CloudWatchClient* cwClient,
                                            PStreamCallbacks* ppStreamCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCanaryStreamCallbacks pCanaryStreamCallbacks = NULL;
    Aws::CloudWatch::Model::Dimension dimension;

    CHK(ppStreamCallbacks != NULL, STATUS_NULL_ARG);

    // Allocate the entire structure
    pCanaryStreamCallbacks = (PCanaryStreamCallbacks) MEMCALLOC(1, SIZEOF(CanaryStreamCallbacks));
    CHK(pCanaryStreamCallbacks != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Set the version, self
    pCanaryStreamCallbacks->streamCallbacks.version = STREAM_CALLBACKS_CURRENT_VERSION;
    pCanaryStreamCallbacks->streamCallbacks.customData = (UINT64) pCanaryStreamCallbacks;
    pCanaryStreamCallbacks->timeOfNextKeyFrame = new std::map<UINT64, UINT64>();

    pCanaryStreamCallbacks->pCwClient = cwClient;


    dimension.SetName("KinesisVideoProducerSDK");
    dimension.SetValue("Producer");

    pCanaryStreamCallbacks->receivedAckDatum.SetMetricName("ReceivedAckLatency");
    pCanaryStreamCallbacks->persistedAckDatum.SetMetricName("PersistedAckLatency");
    pCanaryStreamCallbacks->streamReadyDatum.SetMetricName("StreamReady");
    pCanaryStreamCallbacks->streamErrorDatum.SetMetricName("StreamError");
    pCanaryStreamCallbacks->connectionStaleDatum.SetMetricName("ConnectionStale");
    pCanaryStreamCallbacks->latencyPressureDatum.SetMetricName("LatencyPressure");

    pCanaryStreamCallbacks->receivedAckDatum.AddDimensions(dimension);
    pCanaryStreamCallbacks->persistedAckDatum.AddDimensions(dimension);
    pCanaryStreamCallbacks->streamReadyDatum.AddDimensions(dimension);
    pCanaryStreamCallbacks->streamErrorDatum.AddDimensions(dimension);
    pCanaryStreamCallbacks->connectionStaleDatum.AddDimensions(dimension);
    pCanaryStreamCallbacks->latencyPressureDatum.AddDimensions(dimension);

    // Set callbacks
    pCanaryStreamCallbacks->streamCallbacks.fragmentAckReceivedFn = CanaryStreamFragmentAckHandler;
    pCanaryStreamCallbacks->streamCallbacks.streamConnectionStaleFn = CanaryStreamConnectionStaleHandler;
    pCanaryStreamCallbacks->streamCallbacks.streamErrorReportFn = CanaryStreamErrorReportHandler;
    pCanaryStreamCallbacks->streamCallbacks.streamLatencyPressureFn = CanaryStreamLatencyPressureHandler;
    pCanaryStreamCallbacks->streamCallbacks.streamReadyFn = CanaryStreamReadyHandler;
    pCanaryStreamCallbacks->streamCallbacks.freeStreamCallbacksFn = CanaryStreamFreeHandler;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeCanaryStreamCallbacks((PStreamCallbacks*) &pCanaryStreamCallbacks);
        pCanaryStreamCallbacks = NULL;
    }

    // Set the return value if it's not NULL
    if (ppStreamCallbacks != NULL) {
        *ppStreamCallbacks = (PStreamCallbacks) pCanaryStreamCallbacks;
    }

    LEAVES();
    return retStatus;
}

STATUS freeCanaryStreamCallbacks(PStreamCallbacks* ppStreamCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCanaryStreamCallbacks pCanaryStreamCallbacks = NULL;

    CHK(ppStreamCallbacks != NULL, STATUS_NULL_ARG);

    pCanaryStreamCallbacks = (PCanaryStreamCallbacks) *ppStreamCallbacks;

    // Call is idempotent
    CHK(pCanaryStreamCallbacks != NULL, retStatus);

    delete(pCanaryStreamCallbacks->timeOfNextKeyFrame);
    // Release the object
    MEMFREE(pCanaryStreamCallbacks);

    // Set the pointer to NULL
    *ppStreamCallbacks = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS CanaryStreamFreeHandler(PUINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStreamCallbacks pStreamCallbacks;

    CHK(customData != NULL, STATUS_NULL_ARG);
    pStreamCallbacks = (PStreamCallbacks) *customData;
    CHK_STATUS(freeCanaryStreamCallbacks(&pStreamCallbacks));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS CanaryStreamConnectionStaleHandler(UINT64 customData, STREAM_HANDLE streamHandle,
                                                   UINT64 lastBufferingAck)
{
    UNUSED_PARAM(lastBufferingAck);
    UNUSED_PARAM(streamHandle);
    PCanaryStreamCallbacks pCanaryStreamCallbacks = (PCanaryStreamCallbacks) customData;

    pCanaryStreamCallbacks->connectionStaleDatum.SetValue(1.0);
    pCanaryStreamCallbacks->connectionStaleDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::None);
    CanaryStreamSendMetrics(pCanaryStreamCallbacks, pCanaryStreamCallbacks->connectionStaleDatum);

    return STATUS_SUCCESS;
}

STATUS CanaryStreamErrorReportHandler(UINT64 customData, STREAM_HANDLE streamHandle,
                                               UPLOAD_HANDLE uploadHandle, UINT64 erroredTimecode,
                                               STATUS statusCode)
{
    PCanaryStreamCallbacks pCanaryStreamCallbacks = (PCanaryStreamCallbacks) customData;

    DLOGE("CanaryStreamErrorReportHandler got error %lu at time %" PRIu64 " for stream % " PRIu64 " for upload handle %" PRIu64, statusCode, erroredTimecode, streamHandle, uploadHandle);
    pCanaryStreamCallbacks->streamErrorDatum.SetValue(1.0);
    pCanaryStreamCallbacks->streamErrorDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::None);
    CanaryStreamSendMetrics(pCanaryStreamCallbacks, pCanaryStreamCallbacks->streamErrorDatum);

    return STATUS_SUCCESS;
}

STATUS CanaryStreamLatencyPressureHandler(UINT64 customData, STREAM_HANDLE streamHandle, UINT64 currentBufferDuration)
{
    PCanaryStreamCallbacks pCanaryStreamCallbacks = (PCanaryStreamCallbacks) customData;

    pCanaryStreamCallbacks->latencyPressureDatum.SetValue(1.0);
    pCanaryStreamCallbacks->latencyPressureDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::None);
    CanaryStreamSendMetrics(pCanaryStreamCallbacks, pCanaryStreamCallbacks->latencyPressureDatum);

    return STATUS_SUCCESS;
}

STATUS CanaryStreamReadyHandler(UINT64 customData, STREAM_HANDLE streamHandle)
{
    PCanaryStreamCallbacks pCanaryStreamCallbacks = (PCanaryStreamCallbacks) customData;

    pCanaryStreamCallbacks->streamReadyDatum.SetValue(1.0);
    pCanaryStreamCallbacks->streamReadyDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::None);
    CanaryStreamSendMetrics(pCanaryStreamCallbacks, pCanaryStreamCallbacks->streamReadyDatum);

    return STATUS_SUCCESS;
}

STATUS CanaryStreamFragmentAckHandler(UINT64 customData,
                                      STREAM_HANDLE streamHandle,
                                      UPLOAD_HANDLE uploadHandle,
                                      PFragmentAck ack)
{
    PCanaryStreamCallbacks pCanaryStreamCallbacks = (PCanaryStreamCallbacks) customData;
    UINT64 time = GETTIME();
    UINT64 timeOfFragmentEndSent = pCanaryStreamCallbacks->timeOfNextKeyFrame->find(ack->timestamp)->second;
    if (ack->ackType != FRAGMENT_ACK_TYPE_BUFFERING) {
        DLOGD("ack type %u latency %" PRIu64, ack->ackType, (GETTIME() - timeOfFragmentEndSent) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    switch (ack->ackType) {
        case FRAGMENT_ACK_TYPE_RECEIVED:
            pCanaryStreamCallbacks->receivedAckDatum.SetValue((GETTIME() - timeOfFragmentEndSent) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            pCanaryStreamCallbacks->receivedAckDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
            CanaryStreamSendMetrics(pCanaryStreamCallbacks, pCanaryStreamCallbacks->receivedAckDatum);
            break;
        case FRAGMENT_ACK_TYPE_PERSISTED:
            pCanaryStreamCallbacks->persistedAckDatum.SetValue((GETTIME() - timeOfFragmentEndSent) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            pCanaryStreamCallbacks->persistedAckDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
            CanaryStreamSendMetrics(pCanaryStreamCallbacks, pCanaryStreamCallbacks->persistedAckDatum);
            pCanaryStreamCallbacks->timeOfNextKeyFrame->erase(ack->timestamp);
            break;
        case FRAGMENT_ACK_TYPE_ERROR:
            DLOGE("Received Error Ack timestamp %" PRIu64 " fragment number %s error code %lu", ack->timestamp, ack->sequenceNumber, ack->result);
            break;
        default:
            break;
    }
    DLOGD("CanaryStreamFragmentAckHandler took %" PRIu64, GETTIME() - time);
    return STATUS_SUCCESS;
}

VOID OnPutMetricDataResponseReceivedHandler(const Aws::CloudWatch::CloudWatchClient* cwClient,
                                            const Aws::CloudWatch::Model::PutMetricDataRequest& request,
                                            const Aws::CloudWatch::Model::PutMetricDataOutcome& outcome,
                                            const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) {
    if (!outcome.IsSuccess())
    {
        DLOGE("Failed to put sample metric data: %s", outcome.GetError().GetMessage().c_str());
    }
    else
    {
        DLOGV("Successfully put sample metric data");
    }
}

VOID CanaryStreamSendMetrics(PCanaryStreamCallbacks pCanaryStreamCallbacks, Aws::CloudWatch::Model::MetricDatum& metricDrum)
{
    Aws::CloudWatch::Model::PutMetricDataRequest cwRequest;
    cwRequest.SetNamespace("KinesisVideoSDKCanary");
    cwRequest.AddMetricData(metricDrum);
    pCanaryStreamCallbacks->pCwClient->PutMetricDataAsync(cwRequest, OnPutMetricDataResponseReceivedHandler);
}

VOID CanaryStreamRecordFragmentEndSendTime(PStreamCallbacks pCanaryStreamCallbacks, UINT64 lastKeyFrameTime, UINT64 curKeyFrameTime) {
    auto mapPtr = ((PCanaryStreamCallbacks) pCanaryStreamCallbacks)->timeOfNextKeyFrame;
    (*mapPtr)[lastKeyFrameTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND] = curKeyFrameTime;
    auto iter = mapPtr->begin();
    while (iter != mapPtr->end()) {
        // clean up from current timestamp of 5 min timestamps would be removed
        if (iter->first < (GETTIME() - 300 * HUNDREDS_OF_NANOS_IN_A_SECOND) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND) {
            iter = mapPtr->erase(iter);
        } else {
            break;
        }
    }
}