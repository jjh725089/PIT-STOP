#ifndef RTSPCLIENTTLSINTERACTION_H
#define RTSPCLIENTTLSINTERACTION_H

#ifdef signals
#undef signals
#endif
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(RtspClientTlsInteraction, rtsp_client_tls_interaction, RTSP_CLIENT_TLS, INTERACTION, GTlsInteraction)
#define RTSP_CLIENT_TLS_INTERACTION_TYPE (rtsp_client_tls_interaction_get_type())


RtspClientTlsInteraction *rtsp_client_tls_interaction_new(GTlsCertificate *cert, GTlsCertificate *ca_cert, GTlsDatabase *database);

G_END_DECLS

#endif // RTSPCLIENTTLSINTERACTION_H
