#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include "utility.h" // Including utility header file

// Define constants for seller counts, concert dimensions, and simulation duration
#define hp_seller_count 1
#define mp_seller_count 3
#define lp_seller_count 6
#define total_seller (hp_seller_count + mp_seller_count + lp_seller_count)
#define concert_row 10
#define concert_col 10
#define simulation_duration 60
#define MAX_CUSTOMERS 100

// Global variables
int sim_time;																	// Simulation time
int N = 5;																		// Number of customers
int at1[15] = {0}, st1[15] = {0}, tat1[15] = {0}, bt1[15] = {0}, rt1[15] = {0}; // Arrays for storing metrics
float throughput[3] = {0};														// Array for storing throughput of each seller type
float avg_rt = 0, avg_tat = 0, cust_served = 0;									// Variables for storing average response time and turnaround time
char seat_matrix[concert_row][concert_col][5];
int at_H[MAX_CUSTOMERS] = {0}, tat_H[MAX_CUSTOMERS] = {0}, rt_H[MAX_CUSTOMERS] = {0};
int at_M[MAX_CUSTOMERS] = {0}, tat_M[MAX_CUSTOMERS] = {0}, rt_M[MAX_CUSTOMERS] = {0};
int at_L[MAX_CUSTOMERS] = {0}, tat_L[MAX_CUSTOMERS] = {0}, rt_L[MAX_CUSTOMERS] = {0}; // Matrix representing concert seat arrangement

// Thread variables
pthread_t seller_t[total_seller];												 // Array to store seller threads
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;					 // Mutex for controlling access to thread count
pthread_mutex_t thread_waiting_for_clock_tick_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for waiting for clock tick
pthread_mutex_t reservation_mutex = PTHREAD_MUTEX_INITIALIZER;					 // Mutex for reservation process
pthread_mutex_t thread_completion_mutex = PTHREAD_MUTEX_INITIALIZER;			 // Mutex for thread completion
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;					 // Mutex for condition variable
pthread_cond_t condition_cond = PTHREAD_COND_INITIALIZER;						 // Condition variable for synchronizing threads

// Structure for passing arguments to seller threads
typedef struct sell_arg_struct
{
	char seller_no;
	char seller_type;
	queue *seller_queue;
} sell_arg;

// Structure representing a customer
typedef struct customer_struct
{
	char cust_no;
	int arrival_time;
} customer;

// Function prototypes
void display_queue(queue *q);
void create_seller_threads(pthread_t *thread, char seller_type, int no_of_sellers);
void wait_for_thread_to_serve_current_time_slice();
void wakeup_all_seller_threads();
void *sell(void *);
queue *generate_customer_queue(int);
int compare_by_arrival_time(void *data1, void *data2);
int findAvailableSeat(char seller_type);

// Global counters
int thread_count = 0;
int threads_waiting_for_clock_tick = 0;
int active_thread = 0;
int verbose = 0; // Verbosity flag

// Function to create seller threads
void create_seller_threads(pthread_t *thread, char seller_type, int no_of_sellers)
{
	// Create all threads
	for (int t_no = 0; t_no < no_of_sellers; t_no++)
	{
		// Allocate memory for seller argument structure
		sell_arg *seller_arg = (sell_arg *)malloc(sizeof(sell_arg));
		seller_arg->seller_no = t_no;
		seller_arg->seller_type = seller_type;
		seller_arg->seller_queue = generate_customer_queue(N);

		// Increment thread count
		pthread_mutex_lock(&thread_count_mutex);
		thread_count++;
		pthread_mutex_unlock(&thread_count_mutex);

		// Print thread creation message if verbose mode is enabled
		if (verbose)
			printf("Creating thread %c%02d\n", seller_type, t_no);

		// Create thread
		pthread_create(thread + t_no, NULL, &sell, seller_arg);
	}
}

// Function to display contents of a queue

// Function to wait for all threads to serve current time slice
void wait_for_thread_to_serve_current_time_slice()
{
	while (1)
	{
		pthread_mutex_lock(&thread_waiting_for_clock_tick_mutex);
		if (threads_waiting_for_clock_tick == active_thread)
		{
			threads_waiting_for_clock_tick = 0;
			pthread_mutex_unlock(&thread_waiting_for_clock_tick_mutex);
			break;
		}
		pthread_mutex_unlock(&thread_waiting_for_clock_tick_mutex);
	}
}

void display_queue(queue *q)
{
	for (node *ptr = q->head; ptr != NULL; ptr = ptr->next)
	{
		customer *cust = (customer *)ptr->data;
		printf("[%d,%d]", cust->cust_no, cust->arrival_time);
	}
}

// Function to wake up all seller threads
void wakeup_all_seller_threads()
{
	pthread_mutex_lock(&condition_mutex);
	if (verbose)
		printf("00:%02d Main Thread Broadcasting Clock Tick\n", sim_time);
	pthread_cond_broadcast(&condition_cond);
	pthread_mutex_unlock(&condition_mutex);
}

// Function executed by each seller thread
void *sell(void *t_args)
{
	// Initializing thread
	sell_arg *args = (sell_arg *)t_args;
	queue *customer_queue = args->seller_queue;
	queue *seller_queue = create_queue();
	char seller_type = args->seller_type;
	int seller_no = args->seller_no + 1;

	// Update thread count
	pthread_mutex_lock(&thread_count_mutex);
	thread_count--;
	active_thread++;
	pthread_mutex_unlock(&thread_count_mutex);

	customer *cust = NULL;
	int random_wait_time = 0;
	int temp1 = 0;

	// Main loop for selling tickets
	while (sim_time < simulation_duration)
	{
		// Waiting for clock tick
		pthread_mutex_lock(&condition_mutex);
		if (verbose)
			printf("00:%02d %c%02d Waiting for next clock tick\n", sim_time, seller_type, seller_no);

		pthread_mutex_lock(&thread_waiting_for_clock_tick_mutex);
		threads_waiting_for_clock_tick++;
		pthread_mutex_unlock(&thread_waiting_for_clock_tick_mutex);

		pthread_cond_wait(&condition_cond, &condition_mutex);
		if (verbose)
			printf("00:%02d %c%02d Received Clock Tick\n", sim_time, seller_type, seller_no);
		pthread_mutex_unlock(&condition_mutex);

		// Sell tickets
		if (sim_time == simulation_duration)
			break;

		// Handle arrival of new customers
		while (customer_queue->size > 0 && ((customer *)customer_queue->head->data)->arrival_time <= sim_time)
		{
			customer *temp = (customer *)dequeue(customer_queue);
			enqueue(seller_queue, temp);
			printf("00:%02d %c%d Arrived: Customer No %c%d%02d\n", sim_time, seller_type, seller_no, seller_type, seller_no, temp->cust_no);
		}

		// Serve next customer
		if (cust == NULL && seller_queue->size > 0)
		{
			cust = (customer *)dequeue(seller_queue);
			printf("00:%02d %c%d Serving: Customer No %c%d%02d\n", sim_time, seller_type, seller_no, seller_type, seller_no, cust->cust_no);

			// Determine random wait time based on seller type
			switch (seller_type)
			{
			case 'H':
				random_wait_time = (rand() % 2) + 1;
				bt1[temp1] = random_wait_time;
				temp1++;
				break;
			case 'M':
				random_wait_time = (rand() % 3) + 2;
				bt1[temp1] = random_wait_time;
				temp1++;
				break;
			case 'L':
				random_wait_time = (rand() % 4) + 4;
				bt1[temp1] = random_wait_time;
				temp1++;
			}
			if (seller_type == 'H')
			{
				rt_H[cust->cust_no] = sim_time - cust->arrival_time;					 // Response time calculation
				tat_H[cust->cust_no] = sim_time + random_wait_time - cust->arrival_time; // TAT calculation
			}
			else if (seller_type == 'M')
			{
				rt_M[cust->cust_no] = sim_time - cust->arrival_time;
				tat_M[cust->cust_no] = sim_time + random_wait_time - cust->arrival_time;
			}
			else if (seller_type == 'L')
			{
				rt_L[cust->cust_no] = sim_time - cust->arrival_time;
				tat_L[cust->cust_no] = sim_time + random_wait_time - cust->arrival_time;
			}
		}

		// Process customer if exists
		if (cust != NULL)
		{
			if (random_wait_time == 0)
			{
				// Selling seat
				pthread_mutex_lock(&reservation_mutex);

				// Find available seat
				int seatIndex = findAvailableSeat(seller_type);
				if (seatIndex == -1)
				{
					printf("00:%02d %c%d Sold Out Tickets: Customer No %c%d%02d .\n", sim_time, seller_type, seller_no, seller_type, seller_no, cust->cust_no);
				}
				else
				{
					int row_no = seatIndex / concert_col;
					int col_no = seatIndex % concert_col;
					sprintf(seat_matrix[row_no][col_no], "%c%d%02d", seller_type, seller_no, cust->cust_no);
					printf("00:%02d %c%d Assigned Seat %d,%d to Customer No %c%d%02d  \n", sim_time, seller_type, seller_no, row_no, col_no, seller_type, seller_no, cust->cust_no);
					cust_served++;

					// Update throughput based on seller type
					if (seller_type == 'L')
						throughput[0]++;
					else if (seller_type == 'M')
						throughput[1]++;
					else if (seller_type == 'H')
						throughput[2]++;
				}
				pthread_mutex_unlock(&reservation_mutex);
				cust = NULL;
			}
			else
			{
				random_wait_time--;
			}
		}
	}

	// Process remaining customers
	while (cust != NULL || seller_queue->size > 0)
	{
		if (cust == NULL)
			cust = (customer *)dequeue(seller_queue);
		printf("00:%02d %c%d Ticket Sale Closed. Customer Leaves:  %c%d%02d \n", sim_time, seller_type, seller_no, seller_type, seller_no, cust->cust_no);
		cust = NULL;
	}

	// Update active thread count
	pthread_mutex_lock(&thread_count_mutex);
	active_thread--;
	pthread_mutex_unlock(&thread_count_mutex);
}

// Function to find available seat based on seller type
int findAvailableSeat(char seller_type)
{
	int seatIndex = -1;

	if (seller_type == 'H')
	{
		// Find available seat in high-priced section
		for (int row_no = 0; row_no < concert_row; row_no++)
		{
			for (int col_no = 0; col_no < concert_col; col_no++)
			{
				if (strcmp(seat_matrix[row_no][col_no], "-") == 0)
				{
					seatIndex = row_no * concert_col + col_no;
					return seatIndex;
				}
			}
		}
	}
	else if (seller_type == 'M')
	{
		// Find available seat in medium-priced section
		int mid = (concert_row / 2) - 1;
		int row_jump = 0;
		int next_row_no = mid;
		for (row_jump = 0;; row_jump++)
		{
			int row_no = mid + row_jump;
			if (mid + row_jump < concert_row)
			{
				for (int col_no = 0; col_no < concert_col; col_no++)
				{
					if (strcmp(seat_matrix[row_no][col_no], "-") == 0)
					{
						seatIndex = row_no * concert_col + col_no;
						return seatIndex;
					}
				}
			}
			row_no = mid - row_jump;
			if (mid - row_jump >= 0)
			{
				for (int col_no = 0; col_no < concert_col; col_no++)
				{
					if (strcmp(seat_matrix[row_no][col_no], "-") == 0)
					{
						seatIndex = row_no * concert_col + col_no;
						return seatIndex;
					}
				}
			}
			if (mid + row_jump >= concert_row && mid - row_jump < 0)
			{
				break;
			}
		}
	}
	else if (seller_type == 'L')
	{
		// Find available seat in low-priced section
		for (int row_no = concert_row - 1; row_no >= 0; row_no--)
		{
			for (int col_no = concert_col - 1; col_no >= 0; col_no--)
			{
				if (strcmp(seat_matrix[row_no][col_no], "-") == 0)
				{
					seatIndex = row_no * concert_col + col_no;
					return seatIndex;
				}
			}
		}
	}

	return -1;
}

// Function to generate customer queue with random arrival times
queue *generate_customer_queue(int N)
{
	queue *customer_queue = create_queue();
	char cust_no = 0;
	while (N--)
	{
		customer *cust = (customer *)malloc(sizeof(customer));
		cust->cust_no = cust_no;
		cust->arrival_time = rand() % simulation_duration;
		at1[cust_no] = cust->arrival_time;
		enqueue(customer_queue, cust);
		cust_no++;
	}
	sort(customer_queue, compare_by_arrival_time);
	node *ptr = customer_queue->head;
	cust_no = 0;
	while (ptr != NULL)
	{
		cust_no++;
		customer *cust = (customer *)ptr->data;
		cust->cust_no = cust_no;
		ptr = ptr->next;
	}
	return customer_queue;
}

// Function to compare customers by arrival time
int compare_by_arrival_time(void *data1, void *data2)
{
	customer *c1 = (customer *)data1;
	customer *c2 = (customer *)data2;
	if (c1->arrival_time < c2->arrival_time)
	{
		return -1;
	}
	else if (c1->arrival_time == c2->arrival_time)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

// Main function
int main(int argc, char **argv)
{
	srand(4388); // Seed random number generator

	// Check if N is provided as command-line argument
	if (argc == 2)
	{
		N = atoi(argv[1]); // Set number of customers
	}

	// Initialize seat matrix
	for (int r = 0; r < concert_row; r++)
	{
		for (int c = 0; c < concert_col; c++)
		{
			strncpy(seat_matrix[r][c], "-", 1); // Set all seats as available
		}
	}

	// Create seller threads for each type
	create_seller_threads(seller_t, 'H', hp_seller_count);
	create_seller_threads(seller_t + hp_seller_count, 'M', mp_seller_count);
	create_seller_threads(seller_t + hp_seller_count + mp_seller_count, 'L', lp_seller_count);

	// Wait for threads to finish initialization and synchronize with clock tick
	while (1)
	{
		pthread_mutex_lock(&thread_count_mutex);
		if (thread_count == 0) //all seller thraeds are correctly initialized
		{
			pthread_mutex_unlock(&thread_count_mutex);
			break;
		}
		pthread_mutex_unlock(&thread_count_mutex);
	}

	// Simulate each time slice
	printf("===============================\n");
	printf("Starting Simulation Threads\n");
	printf("===============================\n");
	threads_waiting_for_clock_tick = 0; //before simulation there is no thraed waiting
	wakeup_all_seller_threads(); // For first tick

	do
	{
		// Wake up all threads
		wait_for_thread_to_serve_current_time_slice();
		sim_time = sim_time + 1;
		wakeup_all_seller_threads();
	} while (sim_time < simulation_duration);

	// Wake up all threads to prevent deadlock
	wakeup_all_seller_threads();

	// Wait for all threads to complete
	while (active_thread)
		;

	// Display final concert seat chart and statistics
	printf("\n\n");
	printf("========================\n");
	printf("Final Concert Chart\n");
	printf("========================\n");

	// Count customers in each section
	int h_customers = 0, m_customers = 0, l_customers = 0;
	for (int r = 0; r < concert_row; r++)
	{
		for (int c = 0; c < concert_col; c++)
		{
			if (c != 0)
				printf("\t");
			printf("%5s", seat_matrix[r][c]);
			if (seat_matrix[r][c][0] == 'H')
				h_customers++;
			if (seat_matrix[r][c][0] == 'M')
				m_customers++;
			if (seat_matrix[r][c][0] == 'L')
				l_customers++;
		}
		printf("\n");
	}

	// Display statistics
	printf("\n\n===============\n");
	printf("Stat for N = %02d\n", N);
	printf("===============\n");
	printf(" ============================================\n");
	printf("|%3c | No of Customers | Got Seat | Returned |\n", ' ');
	printf(" ============================================\n");
	printf("|%3c | %15d | %8d | %8d |\n", 'H', hp_seller_count * N, h_customers, (hp_seller_count * N) - h_customers);
	printf("|%3c | %15d | %8d | %8d |\n", 'M', mp_seller_count * N, m_customers, (mp_seller_count * N) - m_customers);
	printf("|%3c | %15d | %8d | %8d |\n", 'L', lp_seller_count * N, l_customers, (lp_seller_count * N) - l_customers);
	printf(" ============================================\n");

	// Calculate and display average metrics
	for (int z1 = 0; z1 < N; z1++)
	{
		int ct = 0;
		ct = st1[z1] + bt1[z1];
		rt1[z1] = abs(st1[z1] - at1[z1]);
		tat1[z1] = abs(ct - at1[z1]);
	}

	for (int j1 = 0; j1 < N; j1++)
	{
		avg_tat += tat1[j1];
		avg_rt += rt1[j1];
	}

	float avg_rt_H = 0, avg_tat_H = 0;
	float avg_rt_M = 0, avg_tat_M = 0;
	float avg_rt_L = 0, avg_tat_L = 0;
	for (int i = 0; i < N; i++)
	{
		avg_rt_H += rt_H[i];
		avg_tat_H += tat_H[i];
		avg_rt_M += rt_M[i];
		avg_tat_M += tat_M[i];
		avg_rt_L += rt_L[i];
		avg_tat_L += tat_L[i];
	}

	// Calculate averages
	avg_rt_H /= N;
	avg_tat_H /= N;
	avg_rt_M /= N;
	avg_tat_M /= N;
	avg_rt_L /= N;
	avg_tat_L /= N;

	printf("\n\n============================================\n");
	printf("Average RT is %.2f\n", avg_rt / N);
	printf("Average TAT is %.2f\n", avg_tat / N);
	printf("Average Response Time H: %.2f\n", avg_rt_H);
	printf("Average Turn-Around Time H: %.2f\n", avg_tat_H);
	printf("Average Response Time M: %.2f\n", avg_rt_M);
	printf("Average Turn-Around Time M: %.2f\n", avg_tat_M);
	printf("Average Response Time L: %.2f\n", avg_rt_L);
	printf("Average Turn-Around Time L: %.2f\n", avg_tat_L);
	printf("Throughput of seller H is %.2f\n", throughput[0] / 60.0);
	printf("Throughput of seller M is %.2f\n", throughput[1] / 60.0);
	printf("Throughput of seller L is %.2f\n", throughput[2] / 60.0);
	printf("============================================\n");
	return 0;
}
