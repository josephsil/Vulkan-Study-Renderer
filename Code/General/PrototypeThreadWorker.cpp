#include "PrototypeThreadWorker.h"

void PrototypeThreadWorker::WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
{
    // std::this_thread::sleep_for(std::chrono::nanoseconds(300)); //Artifically slow down this job
    JobData[work_item_idx] = {thread_idx};
    ResultsReadyAtIndex.enqueue(work_item_idx);
}


/**
 * ON_COMPLETE_FN Executes after the join
 */
void PrototypeThreadWorker::ON_COMPLETE_FN()
{
    printf("Read while work finished \n");
    size_t totalResultsCount = 0;
    int j = 0;
    printf("==== OUTPUT \n ");
    for (auto result : FinalOutputSumPerThread)
    {
        printf("%d: %llu  ", j++, result);
        totalResultsCount += result;
    }
    printf("\nTOTAL: %llu \n", totalResultsCount);
}

/**
 * READ_RESULTS_FN Executes to read all remain results after the join, or can be called earlier 
 */
size_t PrototypeThreadWorker::READ_RESULTS_FN()
{
    size_t dequeueCt = ResultsReadyAtIndex.try_dequeue_bulk(readbackBufferForQueue.begin(),
                                                            readbackBufferForQueue.size());
    for (int i = 0; i < dequeueCt; i++)
    {
        size_t indexReadyToRead = readbackBufferForQueue[i];
        auto& readData = JobData[indexReadyToRead].requestdata;
        FinalOutputSumPerThread[readData]++;
        printf("Reading at... Thread: %d | Index: %llu \n", readData, indexReadyToRead);
    }

    return dequeueCt;
}
