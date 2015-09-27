/*
*************************************************************************
* Ralink Technology Corporation
* 5F., No. 5, Taiyuan 1st St., Jhubei City,
* Hsinchu County 302,
* Taiwan, R.O.C.
*
* (c) Copyright 2012, Ralink Technology Corporation
*
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the                         *
* Free Software Foundation, Inc.,                                       *
* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
*                                                                       *
*************************************************************************/

#include <linux/pci.h>
#include "rt_linux.h"
#include "hps_bluez.h"
#include "rtbt_osabl.h"

void *g_hdev = 0;

static inline unsigned char rtbt_get_pkt_type(struct sk_buff *skb)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
	return bt_cb(skb)->pkt_type;
#else
	return skb->pkt_type;
#endif
}

static inline void rtbt_set_pkt_type(struct sk_buff *skb, unsigned char type)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
	bt_cb(skb)->pkt_type = type;
#else
	skb->pkt_type = type;
#endif
}


int rtbt_hci_dev_ioctl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg)
{
	printk("%s(dev=0x%lx): ioctl cmd=0x%x!\n",
			__FUNCTION__, (ULONG)hdev, cmd);
	
	return -ENOIOCTLCMD;
}

void rtbt_hci_dev_notify(struct hci_dev *hdev, unsigned int evt)
{
	printk("%s(dev=0x%lx): evt=0x%x!\n",
			__FUNCTION__, (ULONG)hdev, evt);
	return;
}


void rtbt_hci_dev_destruct(struct hci_dev *hdev)
{
	printk("-->%s(): dev=0x%lx!\n", __FUNCTION__, (ULONG)hdev);
	
	return;
}


int rtbt_hci_dev_flush(struct hci_dev *hdev)
{
	printk("%s(dev=0x%lx)!\n", __FUNCTION__, (ULONG)hdev);
	return 0;
}

static const char *pkt_type_str[]=
	{"UNKNOWN", "HCI_CMD", "ACL_DATA", "SCO_DATA", "HCI_EVENT", "HCI_VENDOR", "ERROR_TYPE"};

int rtbt_hci_dev_send(struct hci_dev *hdev, struct sk_buff *skb)
{
	//struct hci_dev *hdev = (struct hci_dev *)skb->dev;
	struct rtbt_os_ctrl *os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hdev);
	struct rtbt_hps_ops *hps_ops;
	unsigned char pkt_type;
	int status;
	

	//printk("-->%s():\n", __FUNCTION__);

	pkt_type = rtbt_get_pkt_type(skb);
	//printk("hciName:%s type:%s(%d) len:%d\n",
	//		hdev->name, pkt_type_str[pkt_type],
	//		pkt_type, skb->len);
	
//	if (pkt_type == HCI_COMMAND_PKT)
//		hex_dump("rtbt_hci_dev_send: HCI_CMD", skb->data, skb->len);
//	else if (pkt_type == HCI_SCODATA_PKT)
//		hex_dump("rtbt_hci_dev_send: HCI_SCO", skb->data, skb->len);

	if (!os_ctrl || !os_ctrl->hps_ops) {
		kfree_skb(skb);
		return -1;
	}

	hps_ops = os_ctrl->hps_ops;
	if ((!hps_ops->hci_cmd) ||
		(!hps_ops->hci_acl_data) ||
		(!hps_ops->hci_sco_data)) {
		printk("Err, Null Handler!hci_cmd=0x%p, acl_data=0x%p, sco_data=0x%p!\n",
				hps_ops->hci_cmd, hps_ops->hci_acl_data, hps_ops->hci_sco_data);
		kfree_skb(skb);
		return -1;
	}

	switch (pkt_type) {
		case HCI_COMMAND_PKT:
			status = hps_ops->hci_cmd(os_ctrl->dev_ctrl, skb->data, skb->len);
            if( (hdev!=0) && (status == 0)){ 
			    hdev->stat.cmd_tx++;
            }
            break;
			
		case HCI_ACLDATA_PKT:
			status = hps_ops->hci_acl_data(os_ctrl->dev_ctrl, skb->data, skb->len);
            if( (hdev!=0) && (status == 0)){ 
                hdev->stat.acl_tx++;
            }
            break;
			
		case HCI_SCODATA_PKT:
			printk("-->%s():sco len=%d,time=0x%lx\n", __FUNCTION__, skb->len, jiffies);
			os_ctrl->sco_tx_seq = bt_cb(skb)->control.txseq;
			os_ctrl->sco_time_hci = jiffies;
			
			status = hps_ops->hci_sco_data(os_ctrl->dev_ctrl, skb->data, skb->len);
            if( (hdev!=0) && (status == 0)){ 
                hdev->stat.sco_tx++;
            }
			printk("<--%s():sco done, time=0x%lx\n", __FUNCTION__, jiffies);
			break;
			
		case HCI_VENDOR_PKT:
			break;
	}
    if( (hdev!=0) && (status == 0)){ 
        hdev->stat.byte_tx += skb->len;
    } else {
        hdev->stat.err_tx++;
    }
	kfree_skb(skb);
	
	//printk("<--%s():\n", __FUNCTION__);
	return 0;
}

extern void *g_hdev;
int rtbt_hci_dev_receive(void *bt_dev, int pkt_type, char *buf, int len)
{
	//struct hci_event_hdr hdr;
    //struct hci_dev *hdev = (struct hci_dev *)skb->dev;
    struct hci_dev *hdev = 0;
    struct sk_buff *skb;
	int status;
	//int pkt_len;
	
//printk("-->%s(): receive info: pkt_type=%d(%s), len=%d!\n", __FUNCTION__, pkt_type, pkt_type <= 5 ? pkt_type_str[pkt_type] : "ErrPktType", len);

	switch (pkt_type) {
		case HCI_EVENT_PKT:
			if (len < HCI_EVENT_HDR_SIZE) {
				BT_ERR("event block is too short");
				return -EILSEQ;
			}
			break;

		case HCI_ACLDATA_PKT:
			if (len < HCI_ACL_HDR_SIZE) {
				BT_ERR("data block is too short");
				return -EILSEQ;
			}
			break;

		case HCI_SCODATA_PKT:
			if (len < HCI_SCO_HDR_SIZE) {
				BT_ERR("audio block is too short");
				return -EILSEQ;
			}
			break;
	}

	skb = bt_skb_alloc(len, GFP_ATOMIC);
	if (!skb) {
		printk("%s no memory for the packet", ((struct hci_dev *)bt_dev)->name);
		return -ENOMEM;
	}

	skb->dev = g_hdev;
	rtbt_set_pkt_type(skb, pkt_type);
	memcpy(skb_put(skb, len), buf, len);

if (pkt_type == HCI_SCODATA_PKT)
	printk("-->%s(): send sco data to OS, time=0x%lx\n", __FUNCTION__, jiffies);

    hdev = (struct hci_dev *)skb->dev;
    if(hdev){
        hdev->stat.byte_rx += len;
    }
    
	status = hci_recv_frame(hdev,skb);

//printk("<--%s()\n", __FUNCTION__);

	return status;
}

int rtbt_hci_dev_open(struct hci_dev *hdev)
{
	NTSTATUS status = STATUS_FAILURE;
	struct rtbt_os_ctrl *os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hdev);
	//struct rtbt_hps_ops *hps_ops;

	printk("-->%s()\n", __FUNCTION__);
	
	//if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
	if (test_bit(HCI_RUNNING, &hdev->flags))
		return 0;
	
	if (os_ctrl && os_ctrl->hps_ops && os_ctrl->hps_ops->open)
		status = os_ctrl->hps_ops->open(os_ctrl->dev_ctrl);
	else {
		clear_bit(HCI_RUNNING, &hdev->flags);
		printk("Error: os_ctrl->hps_ops->dev_open is null\n");
	}

	if (status == 0) {
		os_ctrl->sco_tx_seq = -1;
		set_bit(HCI_RUNNING, &hdev->flags);
	}
	printk("<--%s(), status=%d\n", __FUNCTION__, status);
	
	return status;
}

int rtbt_hci_dev_close(struct hci_dev *hdev)
{
	NTSTATUS status = STATUS_FAILURE;
	struct rtbt_os_ctrl *os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hdev);
	
	printk("--->%s()\n", __FUNCTION__);

	if (!test_and_clear_bit(HCI_RUNNING, &(hdev->flags))){
		printk("%s():Directly return due to hdev->flags=0x%lx\n", __FUNCTION__, hdev->flags);
		return 0;
	}
	rtbt_hci_dev_flush(hdev);

	if (os_ctrl && os_ctrl->hps_ops && os_ctrl->hps_ops->close)
		status = os_ctrl->hps_ops->close(os_ctrl->dev_ctrl);
	else
		printk("%s():os_ctrl->hps_ops->close is null!\n", __FUNCTION__);
	
	printk("<---%s()\n", __FUNCTION__);
	
	return status;
}

int rtbt_hps_iface_suspend(IN struct rtbt_os_ctrl *os_ctrl)
{
    struct hci_dev *hdev = os_ctrl->bt_dev;

    hci_suspend_dev(hdev);

    return 0;
}

int rtbt_hps_iface_resume(IN struct rtbt_os_ctrl *os_ctrl)
{
    struct hci_dev *hdev = os_ctrl->bt_dev;

    hci_resume_dev(hdev);

    return 0;
}

int rtbt_hps_iface_detach(IN struct rtbt_os_ctrl *os_ctrl)
{
	struct hci_dev *hdev = os_ctrl->bt_dev;

	printk("--->%s()\n", __FUNCTION__);

    hci_dev_hold(hdev);

    //rtbt_hci_dev_close(hciDev);
	/* un-register HCI device */
	if (!hdev) {
		printk("%s():os_ctrl(%p)->bt_dev is NULL\n", __FUNCTION__, os_ctrl);
		return -1;
	}
		
	/*if (hci_unregister_dev(hdev) < 0)
		printk("Can't unregister HCI device %s\n", hdev->name);
*/
    hci_unregister_dev(hdev);
    hci_dev_put(hdev);

	printk("<---%s():Success\n", __FUNCTION__);
	return 0;
}

int rtbt_hps_iface_attach(IN struct rtbt_os_ctrl *os_ctrl)
{
	struct hci_dev *hdev = os_ctrl->bt_dev;

	printk("--->%s()\n", __FUNCTION__);

    hci_dev_hold(hdev);
    
	/* Register HCI device */
	if (hci_register_dev(hdev) < 0) {
		printk("Can't register HCI device\n");
		return -ENODEV;
	}

	hci_dev_put(hdev);

	printk("<---%s():Success\n", __FUNCTION__);
	return 0;
}

int rtbt_hps_iface_deinit(
	IN int if_type, 
	IN void *if_dev, 
	IN struct rtbt_os_ctrl *os_ctrl)
{
	struct hci_dev *hdev;

	if (if_type == RAL_INF_PCI) {
		hdev = (struct hci_dev *)pci_get_drvdata(if_dev);
		printk("%s():hciDev=0x%p\n", __FUNCTION__, hdev);
	}	
	else
		return FALSE;

	if (hdev)
		hci_free_dev(hdev);
	os_ctrl->bt_dev = NULL;
	
	return TRUE;
}

int rtbt_hps_iface_init(
	IN int if_type, 
	IN void *if_dev, 
	IN struct rtbt_os_ctrl *os_ctrl)
{
	struct hci_dev *hdev;

	printk("--->%s(): if_type=%d\n", __FUNCTION__, if_type);
	
	/* Initialize HCI device */
	hdev = hci_alloc_dev();
	if (!hdev) {
		printk("Can't allocate HCI device\n");
		return -1;
	}

	switch (if_type) {
		case RAL_INF_PCI:
			{
				struct pci_dev *pcidev = (struct pci_dev *)if_dev;
				
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
				hdev->bus = HCI_PCI;
				hdev->dev_type = HCI_BREDR;
#else
				hdev->type = HCI_PCI;
#endif
				pci_set_drvdata(pcidev, hdev);
				SET_HCIDEV_DEV(hdev, &pcidev->dev);
			}
			break;

		default:
			printk("invalid if_type(%d)!\n", if_type);
			hci_free_dev(hdev);
			return -1;
	}
g_hdev=hdev;
	os_ctrl->bt_dev = hdev;
	os_ctrl->if_dev = if_dev;
	os_ctrl->hps_ops->recv = rtbt_hci_dev_receive;
	
	hci_set_drvdata(hdev, os_ctrl);
	hdev->open = rtbt_hci_dev_open;
	hdev->close = rtbt_hci_dev_close;
	hdev->flush = rtbt_hci_dev_flush;
	hdev->send = rtbt_hci_dev_send;
//	hdev->destruct = rtbt_hci_dev_destruct;
//	hdev->ioctl = rtbt_hci_dev_ioctl;

//	hdev->owner = THIS_MODULE;

	printk("<--%s():alloc hdev(0x%lx) done\n", __FUNCTION__, (ULONG)hdev);
	
	return 0;
}

