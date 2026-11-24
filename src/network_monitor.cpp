#include "network_monitor.h"

namespace LV {

NetworkMonitor::NetworkMonitor() {
    Init();
}

NetworkMonitor::~NetworkMonitor() {
    Shutdown();
}

bool NetworkMonitor::Init() {
    if (m_hIcmp == INVALID_HANDLE_VALUE) {
        m_hIcmp = IcmpCreateFile();
    }
    return (m_hIcmp != INVALID_HANDLE_VALUE);
}

void NetworkMonitor::Shutdown() {
    if (m_hIcmp != INVALID_HANDLE_VALUE) {
        IcmpCloseHandle(m_hIcmp);
        m_hIcmp = INVALID_HANDLE_VALUE;
    }
}

double NetworkMonitor::PerformPing() {
    if (m_hIcmp == INVALID_HANDLE_VALUE) {
        m_hIcmp = IcmpCreateFile();
        if (m_hIcmp == INVALID_HANDLE_VALUE) return -1.0;
    }

    // Target 1: Cloudflare 1.1.1.1
    // Target 2: Google 8.8.8.8
    const char* targets[] = { "1.1.1.1", "8.8.8.8" };
    char sendData[] = "LV_PING_PACKET";
    BYTE replyBuffer[sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 64] = { 0 };

    for (const char* ipStr : targets) {
        unsigned long ipAddr = inet_addr(ipStr);
        if (ipAddr == INADDR_NONE) continue;

        DWORD replies = IcmpSendEcho(m_hIcmp, ipAddr, sendData, sizeof(sendData),
            NULL, replyBuffer, (DWORD)sizeof(replyBuffer), 1000 /* 1 sec timeout */);

        if (replies > 0) {
            auto* pEcho = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuffer);
            if (pEcho->Status == IP_SUCCESS) {
                // If round trip is reported as 0 ms, return 1.0 ms for precision
                double rtt = (pEcho->RoundTripTime == 0) ? 1.0 : (double)pEcho->RoundTripTime;
                return rtt;
            }
        }
    }

    return -1.0; // Timeout or unreachable
}

void NetworkMonitor::Collect(TelemetrySnapshot& snapshot) {
    // 1. Collect Bandwidth via GetIfTable2
    PMIB_IF_TABLE2 pTable = NULL;
    if (GetIfTable2(&pTable) == NO_ERROR) {
        uint64_t totalIn = 0;
        uint64_t totalOut = 0;

        for (ULONG i = 0; i < pTable->NumEntries; ++i) {
            const MIB_IF_ROW2& row = pTable->Table[i];

            // Ignore software loopback, tunnels, and inactive adapters
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL) {
                continue;
            }
            if (row.OperStatus != IfOperStatusUp) {
                continue;
            }

            totalIn  += row.InOctets;
            totalOut += row.OutOctets;
        }

        LARGE_INTEGER nowQpc, freq;
        QueryPerformanceCounter(&nowQpc);
        QueryPerformanceFrequency(&freq);

        if (m_firstSample) {
            m_lastInOctets  = totalIn;
            m_lastOutOctets = totalOut;
            m_lastQpc       = nowQpc;
            m_firstSample   = false;
            snapshot.netDownKBs = 0.0;
            snapshot.netUpKBs   = 0.0;
        } else {
            double dt = (double)(nowQpc.QuadPart - m_lastQpc.QuadPart) / (double)freq.QuadPart;
            if (dt > 0.05) {
                uint64_t deltaIn  = (totalIn >= m_lastInOctets)   ? (totalIn - m_lastInOctets)   : 0;
                uint64_t deltaOut = (totalOut >= m_lastOutOctets) ? (totalOut - m_lastOutOctets) : 0;

                snapshot.netDownKBs = ((double)deltaIn / 1024.0) / dt;
                snapshot.netUpKBs   = ((double)deltaOut / 1024.0) / dt;

                m_lastInOctets  = totalIn;
                m_lastOutOctets = totalOut;
                m_lastQpc       = nowQpc;
            }
        }

        FreeMibTable(pTable);
    }

    // 2. Collect Real-Time ICMP Ping
    double pingMs = PerformPing();
    snapshot.netPingMs   = pingMs;
    snapshot.pingSuccess = (pingMs >= 0.0);
}

} // namespace LV
