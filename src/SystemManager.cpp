// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "SystemManager.hpp"

namespace GLVM::ecs
{
    CSystemManager* CSystemManager::pInstance_ = nullptr;
    std::mutex CSystemManager::Mutex_;
    
    CSystemManager::CSystemManager() {}
    
    CSystemManager::~CSystemManager()
    {
		/// The destructor is called by "delete pInstance_", so deleting pInstance_ here again would recurse
		if ( pInstance_ == this )
			pInstance_ = nullptr;
    }
    
    CSystemManager* CSystemManager::GetInstance()
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        if(pInstance_ == nullptr)
        {
            pInstance_ = new CSystemManager();
        }
        return pInstance_;
    }
    
    void CSystemManager::ActivateSystem(ISystem* _System)
    {
        tSystemContainer.Push(_System);
        ++s_iSystem_ID;
    }

	void CSystemManager::DeactivateSystem(DeactivatedSystems system) {
		deactivatedSystems.Push(system);
	}

	void CSystemManager::ReturnSystemToActivatedState(DeactivatedSystems system) {
		for ( unsigned int i = 0; i < deactivatedSystems.GetSize(); ++i ) {
			if ( system == deactivatedSystems[i] ) {
				deactivatedSystems.Remove(i);
				return;
			}
		}
	}
	
    void CSystemManager::Update()
    {
		bool removedSystemFlag  = false;
        for(unsigned int i = 0; i < s_iSystem_ID; ++i) {
			for ( unsigned int j = 0; j < deactivatedSystems.GetSize(); ++j ) {
				if ( (unsigned int)deactivatedSystems[j] == i ) {
					removedSystemFlag = true;
					continue;
				}
			}

			if ( removedSystemFlag ) {
				removedSystemFlag = false;
				continue;
			} else {
				tSystemContainer[i]->Update();
			}
		}
    }
}
