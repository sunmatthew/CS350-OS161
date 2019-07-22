#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersectionLock;

static struct cv *northCV;
static struct cv *westCV;
static struct cv *eastCV;
static struct cv *southCV;

bool volatile waitingAtNorth = false;
bool volatile waitingAtSouth = false;
bool volatile waitingAtEast = false;
bool volatile waitingAtWest = false;

bool volatile northOpen = false;
bool volatile southOpen = false;
bool volatile westOpen = false;
bool volatile eastOpen = false;

int volatile numNorth;
int volatile numSouth;
int volatile numEast;
int volatile numWest;

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables. * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
  // Initiliaze variables
  numNorth = 0;
  numSouth = 0;
  numEast = 0;
  numWest = 0;

  intersectionLock = lock_create("lock");
  if (intersectionLock == NULL) {
    panic("could not create intersection lock");
  }

  northCV = cv_create("northCV");
  if (northCV == NULL) {
    panic("could not create N CV");
  }
  southCV = cv_create("southCV");
  if (southCV == NULL) {
    panic("could not create S CV");
  }
  eastCV = cv_create("eastCV");
  if (eastCV == NULL) {
    panic("could not create E CV");
  }
  westCV = cv_create("westCV");
  if (westCV == NULL) {
    panic("could not create W CV");
  }

  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  lock_destroy(intersectionLock);  

  cv_destroy(northCV);
  cv_destroy(southCV);
  cv_destroy(westCV);
  cv_destroy(eastCV);

}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */
void
intersection_before_entry(Direction origin, Direction destination) {
  (void)destination;

  if (origin == north) {
    lock_acquire(intersectionLock);
      if ((!northOpen && (southOpen || eastOpen || westOpen)) || numSouth > 0 || numEast > 0 || numWest > 0) {
        waitingAtNorth = true;
        cv_wait(northCV, intersectionLock);
      }

      // No one in the intersection right now
      if (numNorth == 0 && numWest == 0 && numEast == 0 && numSouth == 0) {
        northOpen = true;
      }
        
      northOpen = true;
      waitingAtNorth = false;
      numNorth ++;

    lock_release(intersectionLock);
  } else if (origin == south) {
    lock_acquire(intersectionLock);
      if ((!southOpen && (northOpen || eastOpen || westOpen)) || numNorth > 0 || numWest > 0 || numEast > 0) {
        waitingAtSouth = true;
        cv_wait(southCV, intersectionLock);
      }

      // No one in the intersection right now
      if (numNorth == 0 && numWest == 0 && numEast == 0 && numSouth == 0) {
        southOpen = true;
      }
        
      southOpen = true;
      waitingAtSouth = false;
      numSouth ++;

    lock_release(intersectionLock);
  } else if (origin == west) {
    lock_acquire(intersectionLock);
      if ((!westOpen && (northOpen || eastOpen || southOpen)) || numSouth > 0 || numNorth > 0 || numEast > 0) {
        waitingAtWest = true;
        cv_wait(westCV, intersectionLock);
      }

      // No one in the intersection right now
      if (numNorth == 0 && numWest == 0 && numEast == 0 && numSouth == 0) {
        westOpen = true;
      }
        
      westOpen = true;
      waitingAtWest = false;
      numWest ++;

    lock_release(intersectionLock);
  } else if (origin == east) {
    lock_acquire(intersectionLock);
      if ((!eastOpen && (northOpen || westOpen || southOpen)) || numSouth > 0 || numWest > 0 || numNorth > 0) {
        waitingAtEast = true;
        cv_wait(eastCV, intersectionLock);
      }

      // No one in the intersection right now
      if (numNorth == 0 && numWest == 0 && numEast == 0 && numSouth == 0) {
        eastOpen = true;
      }
        
      eastOpen = true;
      waitingAtEast = false;
      numEast ++;

    lock_release(intersectionLock);
  }
}
/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */
void
intersection_after_exit(Direction origin, Direction destination)
{
  /* replace this default implementation with your own implementation */
  (void)destination;
  if (origin == north) {
    lock_acquire(intersectionLock);
      numNorth --;
    if (numNorth == 0 && numWest == 0 && numEast == 0 && numSouth == 0) {
      if (waitingAtEast) {
        eastOpen = true;
        northOpen = false;
        cv_broadcast(eastCV, intersectionLock);
      } else if (waitingAtWest) {
        westOpen = true;
        northOpen = false;
        cv_broadcast(westCV, intersectionLock);
      } else if (waitingAtSouth) {
        southOpen = true;
        northOpen = false;
        cv_broadcast(southCV, intersectionLock);
      } else {
        northOpen = false;
      }
    }

    lock_release(intersectionLock);
  } else if (origin == south) {
    lock_acquire(intersectionLock);
      numSouth --;
    if (numNorth == 0 && numWest == 0 && numEast == 0 && numSouth == 0) {
      if (waitingAtEast) {
        eastOpen = true;
        southOpen = false;
        cv_broadcast(eastCV, intersectionLock);
      } else if (waitingAtWest) {
        westOpen = true;
        southOpen = false;
        cv_broadcast(westCV, intersectionLock);
      } else if (waitingAtNorth) {
        northOpen = true;
        southOpen = false;
        cv_broadcast(northCV, intersectionLock);
      } else {
        southOpen = false;
      }
    }
    lock_release(intersectionLock);
  } else if (origin == east) {
    lock_acquire(intersectionLock);
      numEast --;
    if (numNorth == 0 && numWest == 0 && numEast == 0 && numSouth == 0) {
      if (waitingAtNorth) {
        northOpen = true;
        eastOpen = false;
        cv_broadcast(northCV, intersectionLock);
      } else if (waitingAtWest) {
        westOpen = true;
        eastOpen = false;
        cv_broadcast(westCV, intersectionLock);
      } else if (waitingAtSouth) {
        southOpen = true;
        eastOpen = false;
        cv_broadcast(southCV, intersectionLock);
      } else {
        eastOpen = false;
      }
    }
    lock_release(intersectionLock);
  } else if (origin == west) {
    lock_acquire(intersectionLock);
      numWest --;
    if (numNorth == 0 && numWest == 0 && numEast == 0 && numSouth == 0) {
      if (waitingAtSouth) {
        southOpen = true;
        westOpen = false;
        cv_broadcast(southCV, intersectionLock);
      } else if (waitingAtNorth) {
        northOpen = true;
        westOpen = false;
        cv_broadcast(northCV, intersectionLock);
      } else if (waitingAtEast) {
        eastOpen = true;
        westOpen = false;
        cv_broadcast(eastCV, intersectionLock);
      } else {
        westOpen = false;
      }
    }
    lock_release(intersectionLock);
  }
}
