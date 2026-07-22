#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <stdbool.h>

#define MAX_JOBS 64 // Maximum number of jobs that can be tracked

typedef enum JobState
{
    JOB_RUNNING,
    JOB_DONE,
    JOB_STOPPED
} JobState;

typedef struct
{
    bool active; // Indicates if the job slot is currently in use
    
    int id; // shell job number
    pid_t pgid; // Process group ID of the job

    JobState state; // State of the job

    char *command; // The original command line associated with the job

    bool notified; // Indicates if the user has been notified about the job's state change
} Job;

extern Job jobs[MAX_JOBS]; // Array to hold the jobs
extern int next_job_id; // Next job ID to be assigned

Job *add_job(pid_t pgid, const char *command);
Job *find_job_by_id(int id);
Job *find_job_by_pgid(pid_t pgid);
void remove_job(Job *job);
void print_jobs(void);

void reap_background_jobs(void);

#endif /* JOBS_H */