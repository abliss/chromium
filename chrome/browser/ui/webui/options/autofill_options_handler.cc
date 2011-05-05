// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/autofill_options_handler.h"

#include <vector>

#include "base/logging.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/phone_number_i18n.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/guid.h"
#include "grit/generated_resources.h"
#include "grit/webkit_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Converts a credit card type to the appropriate resource ID of the CC icon.
int CreditCardTypeToResourceID(const std::string& type) {
  if (type == kAmericanExpressCard)
    return IDR_AUTOFILL_CC_AMEX;
  else if (type == kDinersCard)
    return IDR_AUTOFILL_CC_DINERS;
  else if (type == kDiscoverCard)
    return IDR_AUTOFILL_CC_DISCOVER;
  else if (type == kGenericCard)
    return IDR_AUTOFILL_CC_GENERIC;
  else if (type == kJCBCard)
    return IDR_AUTOFILL_CC_JCB;
  else if (type == kMasterCard)
    return IDR_AUTOFILL_CC_MASTERCARD;
  else if (type == kSoloCard)
    return IDR_AUTOFILL_CC_SOLO;
  else if (type == kVisaCard)
    return IDR_AUTOFILL_CC_VISA;

  NOTREACHED();
  return 0;
}

// Converts a credit card type to the appropriate localized card type.
string16 LocalizedCreditCardType(const std::string& type) {
  if (type == kAmericanExpressCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX);
  else if (type == kDinersCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DINERS);
  else if (type == kDiscoverCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DISCOVER);
  else if (type == kGenericCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_GENERIC);
  else if (type == kJCBCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_JCB);
  else if (type == kMasterCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD);
  else if (type == kSoloCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_SOLO);
  else if (type == kVisaCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);

  NOTREACHED();
  return string16();
}

// Returns a dictionary that maps country codes to data for the country.
DictionaryValue* GetCountryData() {
  std::string app_locale = AutofillCountry::ApplicationLocale();
  std::vector<std::string> country_codes;
  AutofillCountry::GetAvailableCountries(&country_codes);

  DictionaryValue* country_data = new DictionaryValue();
  for (size_t i = 0; i < country_codes.size(); ++i) {
    const AutofillCountry country(country_codes[i], app_locale);

    DictionaryValue* details = new DictionaryValue();
    details->SetString("name", country.name());
    details->SetString("postalCodeLabel", country.postal_code_label());
    details->SetString("stateLabel", country.state_label());

    country_data->Set(country.country_code(), details);
  }

  return country_data;
}

// Get the multi-valued element for |type| and return it in |ListValue| form.
void GetValueList(const AutofillProfile& profile,
                  AutofillFieldType type,
                  scoped_ptr<ListValue>* list) {
  std::vector<string16> values;
  profile.GetMultiInfo(type, &values);
  list->reset(new ListValue);
  for (size_t i = 0; i < values.size(); ++i) {
    (*list)->Set(i, Value::CreateStringValue(values[i]));
  }
}

// Set the multi-valued element for |type| from input |list| values.
void SetValueList(const ListValue* list,
                  AutofillFieldType type,
                  AutofillProfile* profile) {
  std::vector<string16> values(list->GetSize());
  for (size_t i = 0; i < list->GetSize(); ++i) {
    string16 value;
    if (list->GetString(i, &value))
      values[i] = value;
  }
  profile->SetMultiInfo(type, values);
}

// Pulls the phone number |index|, |list_value|, and |country_code| from the
// |args| input.
void ExtractPhoneNumberInformation(const ListValue* args,
                                   size_t* index,
                                   ListValue** list_value,
                                   std::string* country_code) {
  double number = 0.0;
  if (!args->GetDouble(0, &number)) {
    NOTREACHED();
    return;
  }
  *index = number;

  if (!args->GetList(1, list_value)) {
    NOTREACHED();
    return;
  }

  if (!args->GetString(2, country_code)) {
    NOTREACHED();
    return;
  }
}

// Searches the |list| for the value at |index|.  If this value is present
// in any of the rest of the list, then the item (at |index|) is removed.
// The comparison of phone number values is done on normalized versions of the
// phone number values.
void RemoveDuplicatePhoneNumberAtIndex(size_t index,
                                       const std::string& country_code,
                                       ListValue* list) {
  string16 new_value;
  list->GetString(index, &new_value);

  bool is_duplicate = false;
  for (size_t i = 0; i < list->GetSize() && !is_duplicate; ++i) {
    if (i == index)
      continue;

    string16 existing_value;
    list->GetString(i, &existing_value);
    is_duplicate = autofill_i18n::PhoneNumbersMatch(new_value,
                                                    existing_value,
                                                    country_code);
  }

  if (is_duplicate)
    list->Remove(index, NULL);
}

void ValidatePhoneArguments(const ListValue* args, ListValue** list) {
  size_t index = 0;
  std::string country_code;
  ExtractPhoneNumberInformation(args, &index, list, &country_code);

  RemoveDuplicatePhoneNumberAtIndex(index, country_code, *list);
}

}  // namespace

AutofillOptionsHandler::AutofillOptionsHandler()
    : personal_data_(NULL) {
}

AutofillOptionsHandler::~AutofillOptionsHandler() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// OptionsPageUIHandler implementation:
void AutofillOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "autofillAddresses", IDS_AUTOFILL_ADDRESSES_GROUP_NAME },
    { "autofillCreditCards", IDS_AUTOFILL_CREDITCARDS_GROUP_NAME },
    { "autofillAddAddress", IDS_AUTOFILL_ADD_ADDRESS_BUTTON },
    { "autofillAddCreditCard", IDS_AUTOFILL_ADD_CREDITCARD_BUTTON },
    { "helpButton", IDS_AUTOFILL_HELP_LABEL },
    { "addAddressTitle", IDS_AUTOFILL_ADD_ADDRESS_CAPTION },
    { "editAddressTitle", IDS_AUTOFILL_EDIT_ADDRESS_CAPTION },
    { "addCreditCardTitle", IDS_AUTOFILL_ADD_CREDITCARD_CAPTION },
    { "editCreditCardTitle", IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION },
#if defined(OS_MACOSX)
    { "auxiliaryProfilesEnabled", IDS_AUTOFILL_USE_MAC_ADDRESS_BOOK },
#endif  // defined(OS_MACOSX)
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "autofillOptionsPage",
                IDS_AUTOFILL_OPTIONS_TITLE);

  SetAddressOverlayStrings(localized_strings);
  SetCreditCardOverlayStrings(localized_strings);
}

void AutofillOptionsHandler::Initialize() {
  personal_data_ = web_ui_->GetProfile()->GetPersonalDataManager();
  personal_data_->SetObserver(this);

  LoadAutofillData();
}

void AutofillOptionsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "removeAddress",
      NewCallback(this, &AutofillOptionsHandler::RemoveAddress));
  web_ui_->RegisterMessageCallback(
      "removeCreditCard",
      NewCallback(this, &AutofillOptionsHandler::RemoveCreditCard));
  web_ui_->RegisterMessageCallback(
      "loadAddressEditor",
      NewCallback(this, &AutofillOptionsHandler::LoadAddressEditor));
  web_ui_->RegisterMessageCallback(
      "loadCreditCardEditor",
      NewCallback(this, &AutofillOptionsHandler::LoadCreditCardEditor));
  web_ui_->RegisterMessageCallback(
      "setAddress",
      NewCallback(this, &AutofillOptionsHandler::SetAddress));
  web_ui_->RegisterMessageCallback(
      "setCreditCard",
      NewCallback(this, &AutofillOptionsHandler::SetCreditCard));
  web_ui_->RegisterMessageCallback(
      "validatePhoneNumbers",
      NewCallback(this, &AutofillOptionsHandler::ValidatePhoneNumbers));
  web_ui_->RegisterMessageCallback(
      "validateFaxNumbers",
      NewCallback(this, &AutofillOptionsHandler::ValidateFaxNumbers));
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManager::Observer implementation:
void  AutofillOptionsHandler::OnPersonalDataLoaded() {
  LoadAutofillData();
}

void AutofillOptionsHandler::OnPersonalDataChanged() {
  LoadAutofillData();
}

void AutofillOptionsHandler::SetAddressOverlayStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("autofillEditAddressTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_CAPTION));
  localized_strings->SetString("fullNameLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_FULL_NAME));
  localized_strings->SetString("companyNameLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COMPANY_NAME));
  localized_strings->SetString("addrLine1Label",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1));
  localized_strings->SetString("addrLine2Label",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2));
  localized_strings->SetString("cityLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CITY));
  localized_strings->SetString("countryLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COUNTRY));
  localized_strings->SetString("phoneLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PHONE));
  localized_strings->SetString("faxLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_FAX));
  localized_strings->SetString("emailLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EMAIL));
  localized_strings->SetString("addNewNamePlaceholder",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_NEW_NAME));
  localized_strings->SetString("addNewPhonePlaceholder",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_NEW_PHONE));
  localized_strings->SetString("addNewFaxPlaceholder",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_NEW_FAX));
  localized_strings->SetString("addNewEmailPlaceholder",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_NEW_EMAIL));

  std::string app_locale = AutofillCountry::ApplicationLocale();
  std::string default_country_code =
      AutofillCountry::CountryCodeForLocale(app_locale);
  localized_strings->SetString("defaultCountryCode", default_country_code);
  localized_strings->Set("autofillCountryData", GetCountryData());
}

void AutofillOptionsHandler::SetCreditCardOverlayStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("autofillEditCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION));
  localized_strings->SetString("nameOnCardLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_NAME_ON_CARD));
  localized_strings->SetString("creditCardNumberLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER));
  localized_strings->SetString("creditCardExpirationDateLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EXPIRATION_DATE));
}

void AutofillOptionsHandler::LoadAutofillData() {
  if (!personal_data_->IsDataLoaded())
    return;

  ListValue addresses;
  for (std::vector<AutofillProfile*>::const_iterator i =
           personal_data_->web_profiles().begin();
       i != personal_data_->web_profiles().end(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue((*i)->guid()));
    entry->Append(new StringValue((*i)->Label()));
    addresses.Append(entry);
  }

  web_ui_->CallJavascriptFunction("AutofillOptions.setAddressList", addresses);

  ListValue credit_cards;
  for (std::vector<CreditCard*>::const_iterator i =
           personal_data_->credit_cards().begin();
       i != personal_data_->credit_cards().end(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue((*i)->guid()));
    entry->Append(new StringValue((*i)->Label()));
    int res = CreditCardTypeToResourceID((*i)->type());
    entry->Append(
        new StringValue(web_ui_util::GetImageDataUrlFromResource(res)));
    entry->Append(new StringValue(LocalizedCreditCardType((*i)->type())));
    credit_cards.Append(entry);
  }

  web_ui_->CallJavascriptFunction("AutofillOptions.setCreditCardList",
                                  credit_cards);
}

void AutofillOptionsHandler::RemoveAddress(const ListValue* args) {
  DCHECK(personal_data_->IsDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveProfile(guid);
}

void AutofillOptionsHandler::RemoveCreditCard(const ListValue* args) {
  DCHECK(personal_data_->IsDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveCreditCard(guid);
}

void AutofillOptionsHandler::LoadAddressEditor(const ListValue* args) {
  DCHECK(personal_data_->IsDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  AutofillProfile* profile = personal_data_->GetProfileByGUID(guid);
  if (!profile) {
    // There is a race where a user can click once on the close button and
    // quickly click again on the list item before the item is removed (since
    // the list is not updated until the model tells the list an item has been
    // removed). This will activate the editor for a profile that has been
    // removed. Do nothing in that case.
    return;
  }

  DictionaryValue address;
  address.SetString("guid", profile->guid());
  scoped_ptr<ListValue> list;
  GetValueList(*profile, NAME_FULL, &list);
  address.Set("fullName", list.release());
  address.SetString("companyName", profile->GetInfo(COMPANY_NAME));
  address.SetString("addrLine1", profile->GetInfo(ADDRESS_HOME_LINE1));
  address.SetString("addrLine2", profile->GetInfo(ADDRESS_HOME_LINE2));
  address.SetString("city", profile->GetInfo(ADDRESS_HOME_CITY));
  address.SetString("state", profile->GetInfo(ADDRESS_HOME_STATE));
  address.SetString("postalCode", profile->GetInfo(ADDRESS_HOME_ZIP));
  address.SetString("country", profile->CountryCode());
  GetValueList(*profile, PHONE_HOME_WHOLE_NUMBER, &list);
  address.Set("phone", list.release());
  GetValueList(*profile, PHONE_FAX_WHOLE_NUMBER, &list);
  address.Set("fax", list.release());
  GetValueList(*profile, EMAIL_ADDRESS, &list);
  address.Set("email", list.release());

  web_ui_->CallJavascriptFunction("AutofillOptions.editAddress", address);
}

void AutofillOptionsHandler::LoadCreditCardEditor(const ListValue* args) {
  DCHECK(personal_data_->IsDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  CreditCard* credit_card = personal_data_->GetCreditCardByGUID(guid);
  if (!credit_card) {
    // There is a race where a user can click once on the close button and
    // quickly click again on the list item before the item is removed (since
    // the list is not updated until the model tells the list an item has been
    // removed). This will activate the editor for a profile that has been
    // removed. Do nothing in that case.
    return;
  }

  DictionaryValue credit_card_data;
  credit_card_data.SetString("guid", credit_card->guid());
  credit_card_data.SetString("nameOnCard",
                             credit_card->GetInfo(CREDIT_CARD_NAME));
  credit_card_data.SetString("creditCardNumber",
                             credit_card->GetInfo(CREDIT_CARD_NUMBER));
  credit_card_data.SetString("expirationMonth",
                             credit_card->GetInfo(CREDIT_CARD_EXP_MONTH));
  credit_card_data.SetString(
      "expirationYear",
      credit_card->GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  web_ui_->CallJavascriptFunction("AutofillOptions.editCreditCard",
                                  credit_card_data);
}

void AutofillOptionsHandler::SetAddress(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  AutofillProfile profile(guid);

  std::string country_code;
  string16 value;
  ListValue* list_value;
  if (args->GetList(1, &list_value))
    SetValueList(list_value, NAME_FULL, &profile);
  if (args->GetString(2, &value))
    profile.SetInfo(COMPANY_NAME, value);
  if (args->GetString(3, &value))
    profile.SetInfo(ADDRESS_HOME_LINE1, value);
  if (args->GetString(4, &value))
    profile.SetInfo(ADDRESS_HOME_LINE2, value);
  if (args->GetString(5, &value))
    profile.SetInfo(ADDRESS_HOME_CITY, value);
  if (args->GetString(6, &value))
    profile.SetInfo(ADDRESS_HOME_STATE, value);
  if (args->GetString(7, &value))
    profile.SetInfo(ADDRESS_HOME_ZIP, value);
  if (args->GetString(8, &country_code))
    profile.SetCountryCode(country_code);
  if (args->GetList(9, &list_value))
    SetValueList(list_value, PHONE_HOME_WHOLE_NUMBER, &profile);
  if (args->GetList(10, &list_value))
    SetValueList(list_value, PHONE_FAX_WHOLE_NUMBER, &profile);
  if (args->GetList(11, &list_value))
    SetValueList(list_value, EMAIL_ADDRESS, &profile);

  if (!guid::IsValidGUID(profile.guid())) {
    profile.set_guid(guid::GenerateGUID());
    personal_data_->AddProfile(profile);
  } else {
    personal_data_->UpdateProfile(profile);
  }
}

void AutofillOptionsHandler::SetCreditCard(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  CreditCard credit_card(guid);

  string16 value;
  if (args->GetString(1, &value))
    credit_card.SetInfo(CREDIT_CARD_NAME, value);
  if (args->GetString(2, &value))
    credit_card.SetInfo(CREDIT_CARD_NUMBER, value);
  if (args->GetString(3, &value))
    credit_card.SetInfo(CREDIT_CARD_EXP_MONTH, value);
  if (args->GetString(4, &value))
    credit_card.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, value);

  if (!guid::IsValidGUID(credit_card.guid())) {
    credit_card.set_guid(guid::GenerateGUID());
    personal_data_->AddCreditCard(credit_card);
  } else {
    personal_data_->UpdateCreditCard(credit_card);
  }
}

void AutofillOptionsHandler::ValidatePhoneNumbers(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  ListValue* list_value = NULL;
  ValidatePhoneArguments(args, &list_value);

  web_ui_->CallJavascriptFunction(
    "AutofillEditAddressOverlay.setValidatedPhoneNumbers", *list_value);
}

void AutofillOptionsHandler::ValidateFaxNumbers(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  ListValue* list_value = NULL;
  ValidatePhoneArguments(args, &list_value);

  web_ui_->CallJavascriptFunction(
      "AutofillEditAddressOverlay.setValidatedFaxNumbers", *list_value);
}
