/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @brief   NetXDuo applicative file - Pure Wi-Fi Template with Auto-Reconnect
  ******************************************************************************
  */
/* USER CODE END Header */

#include "app_netxduo.h"
#include "app_azure_rtos.h"
#include "nx_ip.h"
#include "mx_wifi.h"

TX_THREAD AppMainThread;
TX_SEMAPHORE Semaphore;

NX_PACKET_POOL  AppPool;
NX_IP           IpInstance;
NX_DHCP         DhcpClient;

ULONG   IpAddress;
ULONG   NetMask;

static VOID App_Main_Thread_Entry(ULONG thread_input);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);

UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  CHAR *pointer;

#if (USE_STATIC_ALLOCATION == 1)
  printf("Wi-Fi Template application started..\n");

  nx_system_initialize();

  /* Packet pool */
  tx_byte_allocate(byte_pool, (VOID **) &pointer,  NX_PACKET_POOL_SIZE, TX_NO_WAIT);
  nx_packet_pool_create(&AppPool, "Main Packet Pool", PAYLOAD_SIZE, pointer, NX_PACKET_POOL_SIZE);

  /* IP instance */
  tx_byte_allocate(byte_pool, (VOID **) &pointer, 2 * DEFAULT_MEMORY_SIZE, TX_NO_WAIT);
  nx_ip_create(&IpInstance, "Main Ip instance", NULL_ADDRESS, NULL_ADDRESS, &AppPool, nx_driver_emw3080_entry,
                     pointer, 2 * DEFAULT_MEMORY_SIZE, DEFAULT_MAIN_PRIORITY);

  /* DHCP */
  nx_dhcp_create(&DhcpClient, &IpInstance, "DHCP Client");

  /* ARP & protocols */
  tx_byte_allocate(byte_pool, (VOID **) &pointer, ARP_MEMORY_SIZE, TX_NO_WAIT);
  nx_arp_enable(&IpInstance, (VOID *)pointer, ARP_MEMORY_SIZE);
  nx_icmp_enable(&IpInstance);
  nx_udp_enable(&IpInstance);
  nx_tcp_enable(&IpInstance);

  /* Main Thread */
  tx_byte_allocate(byte_pool, (VOID **) &pointer, THREAD_MEMORY_SIZE, TX_NO_WAIT);
  tx_thread_create(&AppMainThread, "App Main thread", App_Main_Thread_Entry, 0, pointer, THREAD_MEMORY_SIZE,
                         DEFAULT_MAIN_PRIORITY, DEFAULT_MAIN_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  tx_semaphore_create(&Semaphore, "DHCP Semaphore", 0);
#endif

  return ret;
}

static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  tx_semaphore_put(&Semaphore);
}

static VOID App_Main_Thread_Entry(ULONG thread_input)
{
  extern MX_WIFIObject_t *wifi_obj_get(void);
  MX_WIFIObject_t *pWifi = wifi_obj_get();

  nx_ip_address_change_notify(&IpInstance, ip_address_change_notify_callback, NULL);

  /* Initial Connection */
  printf("Connecting to Wi-Fi: %s...\n", WIFI_SSID);

  while (MX_WIFI_Connect(pWifi, WIFI_SSID, WIFI_PASSWORD, MX_WIFI_SEC_WPA2_AES) != MX_WIFI_STATUS_OK)
  {
      printf("Wi-Fi connection failed. Retrying in 5 seconds...\n");
      tx_thread_sleep(5 * NX_IP_PERIODIC_RATE);
  }

  printf("Wi-Fi connected! Starting DHCP...\n");
  nx_dhcp_start(&DhcpClient);

  /* Wait for IP Address */
  tx_semaphore_get(&Semaphore, TX_WAIT_FOREVER);
  nx_ip_address_get(&IpInstance, &IpAddress, &NetMask);
  PRINT_IP_ADDRESS(IpAddress);

  /* Auto-Reconnect Loop */
  while(1)
  {
      if (MX_WIFI_IsConnected(pWifi) == 0) // If disconnected
      {
          printf("Wi-Fi disconnected! Attempting to reconnect...\n");
          if (MX_WIFI_Connect(pWifi, WIFI_SSID, WIFI_PASSWORD, MX_WIFI_SEC_WPA2_AES) == MX_WIFI_STATUS_OK)
          {
              printf("Wi-Fi reconnected successfully!\n");
          }
          else
          {
              printf("Reconnect failed. Retrying...\n");
          }
      }
      /* Check connection status every 2 seconds */
      tx_thread_sleep(2 * NX_IP_PERIODIC_RATE);
  }
}
