#include "stdafx.h"

#include "CPage.h"

STDMETHODIMP CPage::get_Index(int *lpIndex)
{
	*lpIndex = m_iPage;
	return S_OK;
}

STDMETHODIMP CPage::get_IsFlash(VARIANT_BOOL *pbIsFlash)
{
	*pbIsFlash = (m_fIsFlash == TRUE) ? VARIANT_TRUE : VARIANT_FALSE;
	return S_OK;
}

STDMETHODIMP CPage::Read(WORD Address, LPBYTE lpValue)
{
	*lpValue = m_lpData[Address % PAGE_SIZE];
	return S_OK;
}

STDMETHODIMP CPage::Write(WORD Address, BYTE Value)
{
	m_lpData[Address % PAGE_SIZE] = Value;
	return S_OK;
}


