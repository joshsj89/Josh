/*
 * jobs.c - Job management functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "jobs.h"

Job jobs[MAX_JOBS] = {0}; // Array to hold the jobs (active starts as false)
int next_job_id = 1; // Next job ID to be assigned

/*
 *  Function: add_job
 *  -----------------
 *  Adds a new job to the job table.
 *
 * Parameters:
 *  - pgid: The process group ID of the job.
 *  - command: The original command line associated with the job.
 * 
 * Returns:
 *  - A pointer to the newly added Job structure, or NULL if the job table is full.
 * 
 * Note:
 *  - The function allocates memory for the command string using strdup, which should be freed when the job is removed.
*/
Job *add_job(pid_t pgid, const char *command)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (!jobs[i].active)
        {
            jobs[i].id = next_job_id++;
            jobs[i].pgid = pgid;

            jobs[i].state = JOB_RUNNING;

            jobs[i].command = strdup(command);

            jobs[i].process_count = 0; // Initialize process count to 0
            jobs[i].live_processes = 0; // Initialize live process count to 0

            jobs[i].active = true;
            jobs[i].notified = false;

            return &jobs[i];
        }
    }

    fprintf(stderr, "job table full\n");
    return NULL;
}

/*
 *  Function: find_job_by_id
 *  ------------------------
 *  Finds a job by its ID.
 *
 *  Parameters:
 *   - id: The ID of the job to find.
 *
 *  Returns:
 *   - A pointer to the Job structure if found, or NULL if not found.
 */
Job *find_job_by_id(int id)
{
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].active && jobs[i].id == id)
            return &jobs[i];

    return NULL;
}

/*
 *  Function: find_job_by_pid
 *  ------------------------
 *  Finds a job by its PID.
 *
 *  Parameters:
 *   - pid: The PID of the job to find.
 *
 *  Returns:
 *   - A pointer to the Job structure if found, or NULL if not found.
 */
Job *find_job_by_pid(pid_t pid)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (!jobs[i].active)
            continue;
            
        for (int j = 0; j < jobs[i].process_count; j++)
            if (jobs[i].pids[j] == pid)
                return &jobs[i];
    }
    
    return NULL;
}

/*
 *  Function: find_job_by_pgid
 *  --------------------------
 *  Finds a job by its process group ID.
 *
 *  Parameters:
 *   - pgid: The process group ID of the job to find.
 *
 *  Returns:
 *   - A pointer to the Job structure if found, or NULL if not found.
 */
Job *find_job_by_pgid(pid_t pgid)
{
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].active && jobs[i].pgid == pgid)
            return &jobs[i];

    return NULL;
}

/**
 *  Function: remove_job
 *  ---------------------
 *  Removes a job from the job table.
 *  The function frees the memory allocated for the command string and marks the job slot as inactive.
 *
 *  Parameters:
 *   - job: A pointer to the Job structure to remove.
 */
void remove_job(Job *job)
{
    if (job == NULL)
        return;

    free(job->command);
    job->command = NULL;

    job->id = 0;
    job->pgid = 0;

    job->state = JOB_DONE;

    job->process_count = 0;
    job->live_processes = 0;

    job->active = false;
    job->notified = false;
}

/**
 *  Function: print_jobs
 *  ---------------------
 *  Prints a list of all active jobs.
 */
void print_jobs(void)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (!jobs[i].active)
            continue;

        const char *state;

        switch (jobs[i].state)
        {
            case JOB_RUNNING:
                state = "Running";
                break;
            case JOB_STOPPED:
                state = "Stopped";
                break;
            case JOB_DONE:
                state = "Done";
                break;
            default:
                state = "Unknown";
                break;
        }

        printf("[%d] %-10s %s\n", jobs[i].id, state, jobs[i].command);

        if (jobs[i].state == JOB_DONE && !jobs[i].notified)
        {
            jobs[i].notified = true; // Mark the job as notified
            remove_job(&jobs[i]); // Remove the job from the job table
        }
    }
}

/*
 * Function: reap_background_jobs
 * -------------------------------
 * Checks for any completed background jobs and prints their status.
 */
void reap_background_jobs(void)
{
    int status;
    pid_t pid;

    // Use a non-blocking wait to check for any finished background jobs
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        Job *job = find_job_by_pid(pid);
        if (job == NULL)
            continue;

        job->state = JOB_DONE;

        if (WIFEXITED(status)) // Check if the child process exited normally
            printf("[%d] Done\t%s\n", job->id, job->command);
        else if (WIFSIGNALED(status)) // Check if the child process was terminated by a signal
            printf("[%d] Terminated by signal %d\n", job->id, WTERMSIG(status));

        job->state = JOB_DONE;
        job->notified = false;
    }
}
