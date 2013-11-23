
/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPaintOptionsAndroid.h"
#include "SkFlattenableBuffers.h"
#include "SkTDict.h"
#include "SkThread.h"
#include <cstring>

#define SKLANG_ENABLE_MUTEX
#ifdef SKLANG_OPT
SkLangList::SkLangList(const SkLanguage& lang){
    SkLangOptDebugf("SKLANG_OPT SkLangList::SkLangList this=%p", this);
    s = SkLanguage(lang);
    next = NULL;
}

//Global static pointer user to ensure a single instance of the class
SkLanguages*  SkLanguages::m_pInstance = NULL;

SkLanguages*  SkLanguages::getInstance(){
   SkLangOptDebugf("SKLANG_OPT SkLanguages::getInstance");
   if(SkUnlikely(!m_pInstance))
      m_pInstance = new SkLanguages();

   return m_pInstance;
}

SkLanguages::SkLanguages(){
    SkLangOptDebugf("SKLANG_OPT SkLanguages::SkLanguages this=%p", this);
    LocaleArray  = NULL;
#ifdef SKLANG_ENABLE_MUTEX
    pthread_mutex_init(&update_mutex, NULL);
#endif
}

SkLangList* SkLanguages::setLanguage( const SkLanguage& lang ){
startlang:
    SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage this=%p", this);
    if( SkUnlikely(!LocaleArray) ){
#ifdef SKLANG_ENABLE_MUTEX
        pthread_mutex_lock( &update_mutex );
#endif
        if( SkUnlikely(!LocaleArray) ){
            LocaleArray = new SkLangList(lang);
#ifdef SKLANG_ENABLE_MUTEX
            pthread_mutex_unlock( &update_mutex );
#endif
            SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage Create LocaleArray=%p", LocaleArray);
            return LocaleArray;
        } else {
#ifdef SKLANG_ENABLE_MUTEX
            pthread_mutex_unlock( &update_mutex );
#endif
            goto startlang;
        }
    }

    SkLangList* l = LocaleArray;
    SkLangList* prev = LocaleArray;
    while( l ){
        if( l->s == lang ){
            SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage Found a match returning");
            return l;
        }
        prev = l;
        l = l->next;
    }

#ifdef SKLANG_ENABLE_MUTEX
    pthread_mutex_lock( &update_mutex );

    //SkLangOptDebugf("SKLANG_OPT new locale %s", lang.getTag().c_str());
    //Within mutex, restart from beginning
    l = LocaleArray;
    prev = LocaleArray;
    while( l ){
        if( l->s == lang ){
            pthread_mutex_unlock( &update_mutex );
            SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage Found a match returning inside LOCK");
            return l;
        }
        prev = l;
        l = l->next;
    }
#endif

    SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage Create a new SkLanglist and add and ret");
    l = new SkLangList(lang);
    prev->next = l;

#ifdef SKLANG_ENABLE_MUTEX
    pthread_mutex_unlock( &update_mutex );
#endif
    return l;
}

SkLangList* SkLanguages::setLanguage( const char* langTag ){
startlangtag:
    SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage with Tag this=%p", this);
    if( SkUnlikely(!LocaleArray) ){
#ifdef SKLANG_ENABLE_MUTEX
        pthread_mutex_lock( &update_mutex );
#endif
        if( SkUnlikely(!LocaleArray) ){
            SkLanguage newLang = SkLanguage(langTag); //Create a new SkLanguage
            LocaleArray = new SkLangList(newLang);
#ifdef SKLANG_ENABLE_MUTEX
            pthread_mutex_unlock( &update_mutex );
#endif
            SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage with Tag Create LocaleArray=%p", LocaleArray);
            return LocaleArray;
        } else {
#ifdef SKLANG_ENABLE_MUTEX
            pthread_mutex_unlock( &update_mutex );
#endif
            goto startlangtag;
        }
    }

    SkLangList* l = LocaleArray;
    SkLangList* prev = LocaleArray;
    while( l ){
        if( l->s.getTag().equals(langTag) ){
            SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage with Tag Found a match returning");
            return l;
        }
        prev = l;
        l = l->next;
    }

#ifdef SKLANG_ENABLE_MUTEX
    pthread_mutex_lock( &update_mutex );

    //Within mutex, restart from beginning
    l = LocaleArray;
    prev = LocaleArray;
    while( l ){
        if( l->s.getTag().equals(langTag) ){
            pthread_mutex_unlock( &update_mutex );
            SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage with Tag Found a match returning inside LOCK");
            return l;
        }
        prev = l;
        l = l->next;
    }
#endif

    SkLangOptDebugf("SKLANG_OPT SkLanguages::setLanguage with Tag Create a new SkLanglist and add and ret");
    SkLanguage newLang = SkLanguage(langTag); //Create a new SkLanguage
    l = new SkLangList(newLang);
    prev->next = l;

#ifdef SKLANG_ENABLE_MUTEX
    pthread_mutex_unlock( &update_mutex );
#endif
    return l;
}
#endif

SkLanguage SkLanguage::getParent() const {
    SkASSERT(!fTag.isEmpty());
    const char* tag = fTag.c_str();

    // strip off the rightmost "-.*"
    const char* parentTagEnd = strrchr(tag, '-');
    if (parentTagEnd == NULL) {
        return SkLanguage();
    }
    size_t parentTagLen = parentTagEnd - tag;
    return SkLanguage(tag, parentTagLen);
}

void SkPaintOptionsAndroid::flatten(SkFlattenableWriteBuffer& buffer) const {
    buffer.writeUInt(fFontVariant);
#ifdef SKLANG_OPT
    buffer.writeString(getLanguage().getTag().c_str());
#else
    buffer.writeString(fLanguage.getTag().c_str());
#endif
    buffer.writeBool(fUseFontFallbacks);
}

void SkPaintOptionsAndroid::unflatten(SkFlattenableReadBuffer& buffer) {
    fFontVariant = (FontVariant)buffer.readUInt();
    SkString tag;
    buffer.readString(&tag);
#ifdef SKLANG_OPT
    setLanguage(tag);
#else
    fLanguage = SkLanguage(tag);
#endif
    fUseFontFallbacks = buffer.readBool();
}

#ifdef SKLANG_OPT
void SkPaintOptionsAndroid::setLanguage(const SkLanguage& language) {
    SkLangOptDebugf("SKLANG_OPT s1 this=%p", this);
    fpLanguage = SkLanguages::getInstance()->setLanguage(language);
}

void SkPaintOptionsAndroid::setLanguage(const char* languageTag) {
   SkLangOptDebugf("SKLANG_OPT s2 this=%p", this);
   fpLanguage = SkLanguages::getInstance()->setLanguage(languageTag);
}

const SkLanguage& SkPaintOptionsAndroid::getLanguage() const {
    if( SkUnlikely (fpLanguage == NULL)){
        SkLangOptDebugf("SKLANG_OPT g1 this=%p", this);
        //Add the default empty language
        //We shouldn't go through this path anyway
        SkLanguage emptyLang = SkLanguage("");
        //We can NOT assign the return pointer to fpLanguage as the API is
        //defined on the READ-ONLY object.
        SkLangList*  fpl;
        fpl = SkLanguages::getInstance()->setLanguage(emptyLang);
        //This is persistent object in the global array,
        //So there is no memory leak
        return fpl->s;
    } else {
        SkLangOptDebugf("SKLANG_OPT g2 this=%p", this);
        return fpLanguage->s;
    }
}
#endif
