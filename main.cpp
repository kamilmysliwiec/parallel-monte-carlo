#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <vector>

using namespace std;
CRITICAL_SECTION cs;

class Lock
{
	public:
		Lock(CRITICAL_SECTION&);
		~Lock();

	private:
		CRITICAL_SECTION& cs;
};

Lock::Lock(CRITICAL_SECTION& cs) : cs(cs)
{
	EnterCriticalSection(&cs);
}

Lock::~Lock()
{
	LeaveCriticalSection(&cs);
}

class Observable
{
	public:
		virtual void notify(const int) = 0;
};

class Observer
{
	public:
		virtual void onNext(const int) = 0;
		virtual void onComplete() const = 0;
};

class ObservableThread : public Observable
{
	public:
		ObservableThread(Observer&, const int, const int);
		~ObservableThread();

		void notify(const int);
		void run();

		inline const HANDLE getHandle() const { return handle; }

	private:
		static DWORD WINAPI start(void *);

		HANDLE handle;
		Observer& observer;
		int id, count, result;
};

ObservableThread::ObservableThread(Observer& observer, const int id, const int count) :
	observer(observer), 
	handle(NULL), 
	count(count), 
	result(0), 
	id(id)
{}

ObservableThread::~ObservableThread()  
{
	if (handle != NULL) CloseHandle(handle);
}

void ObservableThread::run()
{
	handle = CreateThread(
		NULL, 0, 
		&ObservableThread::start, 
		static_cast<void*>(this), 
		0, NULL);

	if (handle == NULL) throw exception();
}

void ObservableThread::notify(const int val)
{
	observer.onNext(val);
}

class ParallelMCSolver : public Observer
{
	public:
		ParallelMCSolver(const int, const int);
		~ParallelMCSolver();

		static const double random();
		static const bool isInArea(const double, const double, const double);

		void solve();
		void waitAll() const;

		void onNext(const int);
		void onComplete() const;

	private:
		int nThreads, nRunning, count, in;
		std::vector<ObservableThread> threads;
		HANDLE * handlers;
};

ParallelMCSolver::ParallelMCSolver(const int nThreads, const int count) :
	nThreads(nThreads), 
	count(count), 
	in(0)
{
	if (nThreads <= 0 || count <= 0) throw exception();

	const int tCount = count / nThreads;
	handlers = new HANDLE[nThreads];
	for (int i = 0; i < nThreads; i++)
	{
		threads.push_back(ObservableThread(*this, i, tCount));
	}
}

ParallelMCSolver::~ParallelMCSolver()
{
	delete[] handlers;
}

const double ParallelMCSolver::random()
{
	return (double)rand() / (RAND_MAX + 1);
}

const bool ParallelMCSolver::isInArea(const double x, const double y, const double r)
{
	return ((x * x) + (y * y)) <= (r * r);
}

///
void ParallelMCSolver::onNext(const int value)
{
	{
		Lock lock(cs);
		in += value;
		if (--nRunning == 0) onComplete();
	}
}

///
void ParallelMCSolver::onComplete() const
{
	double PI = 4.0 * (in / (double)count);
	cout << "        PI: " << PI << endl << endl; 
}

void ParallelMCSolver::solve()
{
	nRunning = nThreads;
	int i = 0;
	for (vector<ObservableThread>::iterator it = threads.begin(); 
		it != threads.end(); it++, i++)
	{
		it->run();
		handlers[i] = it->getHandle();
	} 
}

void ParallelMCSolver::waitAll() const
{
	WaitForMultipleObjects(nThreads, handlers, true, INFINITE);
}

///
DWORD WINAPI ObservableThread::start(void * arg)
{
	ObservableThread * thread = static_cast<ObservableThread*>(arg);
	double x, y;
	srand(time(NULL) * thread->id); 
	 
	int in = 0;
	for (int i = 0; i < thread->count; i++)
	{
		x = ParallelMCSolver::random();
		y = ParallelMCSolver::random();

		if (ParallelMCSolver::isInArea(x, y, 1.0)) ++in;
	}
	thread->notify(in);
	ExitThread(0);
	return 0;
}

int main()
{
	const int count = 10000000;
	int nThreads;

	InitializeCriticalSection(&cs);
	cout << fixed << setprecision(8);
	try
	{
		while (true)
		{
			cout << "        Threads: ";
			cin >> nThreads;

			ParallelMCSolver instance(nThreads, count);

			instance.solve();

			/*
				...
				background stuff...
				...
			*/

			instance.waitAll();
		}
	}
	catch (exception& e)
	{
		cout << e.what() << endl;
	}

	DeleteCriticalSection(&cs);
	cin.get();
	return 0;
}

