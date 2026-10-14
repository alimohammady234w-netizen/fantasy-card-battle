#include "FCBCardDefinition.h"
bool UFCBCardDefinition::IsValidDefinition(FString& OutError) const {
	if (CardId.IsNone()) { OutError = TEXT("Missing CardId"); return false; }
	if (DisplayName.IsEmpty()) { OutError = TEXT("Missing DisplayName"); return false; }
	if (!Attributes.IsValid()) { OutError = TEXT("Attributes must be in 1..100"); return false; }
	if (Artwork.IsNull()) { OutError = TEXT("Missing artwork (card will render with fallback frame)"); return false; }
	return true;
}
