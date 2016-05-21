/*
 * =====================================================================================
 *
 *       Filename: dbpod.hpp
 *        Created: 05/20/2016 14:31:19
 *  Last Modified: 05/20/2016 16:29:40
 *
 *    Description: so many db interaction, so enable the multi-thread support
 *                 when got a DBRecord, the corresponding DBConnection would
 *                 be locked, this is the reason I have to introduce DBHDR, by
 *                 the way I hate the name DBPod<N>::DBHDR
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#pragma once
#include "dbrecord.hpp"
#include "dbconnection.hpp"

// ConnSize gives the number of internal connection handler
//       0 : invalid
//       1 : single thread
//    >= 2 : support multi-thread
//
template<ConnSize = 4>
class DBPod final
{
    private:
        // the lock is on DBConnection and its DBRecordPN both, so here
        // we just use a un-locking PN
        using DBRecordPN = MemoryBlockPN<sizeof(DBRecord), 1024, 1>;
        class InnDeleter
        {
            private:
                DBRecordPN *m_DBRPN;

            public:
                InnDeleter(std::mutex *pLock = nullptr, DBRecordPN *pDBRPN = nullptr)
                    : m_Lock(pLock)
                    , m_DBRPN(pDBRPN)
                {}

                // this class won't own any resource
                // so just use default destructor
                ~InnDeleter() = default;

            void operator()(DBRecord *pBuf)
            {
                // 0. screen out null operation
                if(!pBuf){ return; }

                // 1. free the buffer allocated from the corresponding PN
                if(m_DBRPN){ m_DBRPN->Free(pBuf); }

                // 2. unlock the corresponding DBConnection
                //    not a good design since lock() / unlock() are in different scope
                //    be careful when using it
                if(ConnSize > 1 && m_Lock){ m_Lock->unlock(); }
            }
        };

    public:
        using DBHDR = unique_ptr<DBRecord, InnDeleter>;

    private:
        std::mutex *m_LockV[ConnSize];
        DBRecordPN m_DBRPNV[ConnSize];
        DBConnection *m_DBConnV[ConnSize];

    public:
        // I didn't check validation of connection here
        DBPod(const char *szHostName, const char *szUserName,
                const char *szPassword, const char *szDBName, unsigned int nPort)
            : m_Count(0)
        {
            static_assert(ConnSize > 0,
                    "DBPod should contain at least one connection handler");

            for(int nIndex = 0; nIndex < ConnSize; ++nIndex){
                m_DBConnV[nIndex] = new DBConnection(
                        szHostName, szUserName, szPassword, szDBName, nPort);
                if(ConnSize > 1){
                    m_LockV[nIndex] = new std::mutex();
                }
            }
        }

        // make sure it's non-throw
        DBHDR InnCreateDBHDR(size_t nPodIndex)
        {
            DBRecord *pRecord;
            try{
                pRecord = m_DBConnV[nPodIndex]->CreateDBRecord();
            }catch(...){
                pRecord = nullptr;
            }

            return DBHDR(pRecord, InnDeleter(&(m_LockV[nPodIndex]), &(m_DBRPN[nPodIndex])));
        }

        DBHDR CreateDBHDR()
        {
            // for single thread only, we don't need the lock protection
            if(ConnSize == 1){ return InnCreateDBHDR(0); }

            // ok we need to handle multi-thread
            size_t nIndex = (m_Count++) % ConnSize;
            while(true){
                if(m_LockV[nIndex]->try_lock()){
                    // see here, we don't unlock when return
                    // it unlocks when HDR is destructing
                    return InnCreateDBRecord(nIndex);
                }

                nIndex = (nIndex + 1) % ConnSize;
            }

            // never be here
            return DBHDR(nullptr, InnDeleter(nullptr));
        }
};
