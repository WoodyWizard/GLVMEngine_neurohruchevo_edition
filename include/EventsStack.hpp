// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef STACK
#define STACK

#include "Event.hpp"
#include <cassert>
#include <iostream>

extern GLVM::core::CStack Input_Stack_;

namespace GLVM::core
{
	class CStack
	{
		int iHead_ = 0;
		static const int iStack_Range_ = 6;
		EEvents aStack_[iStack_Range_] = {};
	public:
		void Push(const EEvents& _Event)
		{
			for(int i = 0; i < iHead_; ++i) {
				if(aStack_[i] == _Event)
					return;
			}

			if(iHead_ == iStack_Range_)
				return;
		
			aStack_[iHead_] = _Event;
				
			++iHead_;
		}

		/// Last pushed event. Must not be called on empty stack.
		EEvents& Pop()
		{
			assert( iHead_ > 0 );
			return aStack_[iHead_-1];
		}

		/// Last pushed event or eDEFAULT if the stack is empty
		EEvents Top() const
		{
			return iHead_ > 0 ? aStack_[iHead_-1] : EEvents::eDEFAULT;
		}

		void Remove(const EEvents& _Event)
		{
			EEvents aTemp_Stack[iStack_Range_] = {};
			bool removeFlag = false;
			int n = 0;
		
			for(int j = 0; j < iStack_Range_; ++j)
				aTemp_Stack[j] = aStack_[j];

			for(int i = 0; i < iHead_; ++i)
			{
				if(_Event == aTemp_Stack[i]) {
					removeFlag = true;
					continue;
				}

				aStack_[n] = aTemp_Stack[i];
				++n;
			}

			if ( removeFlag ) {
				--iHead_;
				aStack_[iHead_] = EEvents::eDEFAULT;
			}
		}

        ///<       !!!!!!!!!!!!!!!!!!!!!!!!!!! DELETE ALL THIS IF'S WHITH CHECKEVENT FUNCTION !!!!!!!!!!!!!!!!!!!!!!!!!!
        
		void ControlInput(CEvent& _eEvent)
		{
            if(!(SearchElement(_eEvent.GetEvent()) == eEmpty))
                return;
			
			switch(_eEvent.GetEvent())
			{
			case eGAME_LOOP_KILL:
                Push(eGAME_LOOP_KILL);
				break;
			case eKEYRELEASE_A:
				Remove(eMOVE_LEFT);
				break;
			case eKEYRELEASE_D:
				Remove(eMOVE_RIGHT);
				break;
			case eKEYRELEASE_S:
				Remove(eMOVE_BACKWARD); 
				break;
			case eKEYRELEASE_W:
				Remove(eMOVE_FORWARD); 
				break;
			case eINVENTORY_RELEASE:
				Remove(eINVENTORY);
				break;
            case eKEYRELEASE_JUMP:
                Remove(eJUMP);
                break;
            case eMOUSE_LEFT_BUTTON_RELEASE:
                Remove(eMOUSE_LEFT_BUTTON);
                break;
			case eMOVE_LEFT:
                Push(eMOVE_LEFT);
				break;
			case eMOVE_RIGHT:
                Push(eMOVE_RIGHT);
				break;
			case eMOVE_BACKWARD:
                Push(eMOVE_BACKWARD);
				break;
			case eMOVE_FORWARD:
                Push(eMOVE_FORWARD);
				break;
            case eJUMP:
                Push(eJUMP);
                break;
			case eINVENTORY:
				Push(eINVENTORY);
				break;
			case eDEBUG_COLLISIONS_ACTIVE:
				Push(eDEBUG_COLLISIONS_ACTIVE);
				break;
			case eDEBUG_COLLISIONS_INACTIVE:
				/// Key release: nothing is pushed (nobody removes this event, it would occupy a stack slot forever)
				break;
            case eMOUSE_LEFT_BUTTON:
                Push(eMOUSE_LEFT_BUTTON);
                break;
			default:
				break;
			}
		}

        EEvents SearchElement(EEvents _element)
        {
            for(int i = 0; i < iHead_; ++i)
            {
                if(aStack_[i] == _element)
                    return _element;
            }

            return eEmpty;
        }
        
        EEvents& operator[](int _iIndex)
        {
            return aStack_[_iIndex];
        }

        void PrintStack()
        {
            for(int i = 0; i < 5; ++i)
                std::cout << "Stack: " << aStack_[i] << std::endl;
        }

        bool CheckEvent(EEvents _element)
        {
            for(int i = 0; i < iHead_; ++i)
            {
                if(aStack_[i] == _element)
                    return true;
            }
            return false;
        }

		void Clear() {
			for ( int i = 0; i < iHead_; ++i)
				aStack_[i] = EEvents::eDEFAULT;
		}
	};
}
#endif
