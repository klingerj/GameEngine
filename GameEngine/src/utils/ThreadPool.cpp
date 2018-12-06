#include "ThreadPool.h"

// Define extern threadpool object
ThreadPool threadPool = ThreadPool();

// Atomically enqueue a new job
void ThreadPool::EnqueueJob(thread_job_t job) {
    {
        std::unique_lock <std::mutex> lock(mutex_queue);
        jobs.push(job);
    }
    cv_queue.notify_one();
}

// Get a job from the queue - should be called while the queue mutex is locked
thread_job_t ThreadPool::DequeueJob() {
    thread_job_t job;
    job = jobs.front();
    jobs.pop();
    return job;
}

void ThreadPool::ThreadDoJob(thread_job_t threadJob) {
    threadJob.function(threadJob.data);
}

// Infinite loop - thread only gets launched once.
void ThreadPool::ThreadFunction() {
    while (true) {
        thread_job_t job;
        {
            std::unique_lock<std::mutex> lock(mutex_queue);
            cv_queue.wait(lock, [this] { return !jobs.empty() || (jobs.empty() && quit); });
            if (quit) return;
            job = DequeueJob();
        }
        ThreadDoJob(job);
        job.complete = true;
    }
}

void ThreadPool::JoinThreads() {
    StopThreadJobs();
    for (uint32_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }
}

void ThreadPool::StopThreadJobs() {
    quit = true;
    {
        std::unique_lock<std::mutex> lock(mutex_queue);
        while (!jobs.empty()) {
            jobs.pop();
        }
    }
    cv_queue.notify_all();
}
