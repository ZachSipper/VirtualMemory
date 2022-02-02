#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "API.h"
#include "list.h"

struct Node *head = NULL;

int fifo()
{
	struct Node *temp = head;
	head = list_remove_head(head);

	return temp->data;
}

int lru()
{
	struct Node *temp = head;
	list_remove_head(head);

	return temp->data;
}

int clock()
{

	return 0;
}

/*========================================================================*/

int find_replacement()
{
	int PFN;
	if (replacementPolicy == ZERO)
		PFN = 0;
	else if (replacementPolicy == FIFO)
		PFN = fifo();
	else if (replacementPolicy == LRU)
		PFN = lru();
	else if (replacementPolicy == CLOCK)
		PFN = clock();

	return PFN;
}

int pagefault_handler(int pid, int VPN, char type)
{
	int PFN;

	// find a free PFN.
	PFN = get_freeframe();

	if (PFN >= 0) // free frame is available
	{

		swap_in(pid, VPN, PFN); // load the page to the frame

		PTE pte = read_PTE(pid, VPN); // load the page table entry using the page number(VPN)
		pte.valid = true;			  // update valid status
		pte.PFN = PFN;				  // update the frame that the page points to
		write_PTE(pid, VPN, pte);	  // save change

		IPTE ipte = read_IPTE(PFN); // load the inverse page table using the PFN
		ipte.VPN = VPN;				// update the VPN to point to the correct page
		write_IPTE(PFN, ipte);		// save change

	} // if

	// no free frame available. find a victim using page replacement. ;
	if (PFN < 0)
	{

		PFN = find_replacement();

		IPTE ipte_victim = read_IPTE(PFN);				 // open the inverse page entry to get the viticim's page number
		PTE pte_victim = read_PTE(pid, ipte_victim.VPN); // using the page number, open the victim's page

		swap_out(pid, ipte_victim.VPN, PFN); // save the dirty frame to the disk

		pte_victim.valid = false;					 // update the status of valid to false
		write_PTE(pid, ipte_victim.VPN, pte_victim); // save the PTE updates

		swap_in(pid, VPN, PFN); // load page(VPN) into the newly freed frame(PFN)

		PTE pte_new = read_PTE(pid, VPN); // load the desired page
		pte_new.valid = true;			  // update status of valid
		pte_new.PFN = PFN;				  // update PFN
		write_PTE(pid, VPN, pte_new);	  // save changes

		IPTE ipte_new = read_IPTE(pte_new.PFN);
		ipte_new.VPN = VPN;
		write_IPTE(pte_new.PFN, ipte_new);

	} // if

	if (replacementPolicy == FIFO)
		head = list_insert_tail(head, PFN); // adds new node to the tail of list in case of FIFO
	if (replacementPolicy == LRU)
		head = list_insert_tail(head, PFN); // adds new node to the tail so that the least recently used nodes come first

	return PFN;
}

int is_page_hit(int pid, int VPN, char type)
{
	/* Read page table entry for (pid, VPN) */
	PTE pte = read_PTE(pid, VPN);

	/* if PTE is valid, it is a page hit. Return physical frame number (PFN) */
	if (pte.valid)
	{
		/* Mark the page dirty, if it is a write request */
		if (type == 'W')
		{
			pte.dirty = true;
			write_PTE(pid, VPN, pte);
		}

		IPTE ipte = read_IPTE(pte.PFN);
		ipte.VPN = VPN;
		write_IPTE(pte.PFN, ipte);

		if(type == 'R' && replacementPolicy == LRU)
		{
			lru();
			head = list_insert_tail(head, pte.PFN);
		} // if

		return pte.PFN;
	} // if

	/* "miss" or PageFault, if the PTE is invalid. Return -1 */
	return -1;
}

int MMU(int pid, int VPN, char type, bool *hit)
{
	int PFN;

	// hit
	PFN = is_page_hit(pid, VPN, type);
	if (PFN >= 0)
	{
		stats.hitCount++;
		*hit = true;
		return PFN;
	}

	stats.missCount++;
	*hit = false;

	// miss -> pagefault
	PFN = pagefault_handler(pid, VPN, type);

	return PFN;
}
