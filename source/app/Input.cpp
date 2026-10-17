#include "AppState.hpp"

bool procesarEntrada(u32 kDown, u32 kNav, u32 kUp, touchPosition touch, u64& ultimoCheckTiempo) {
        // --- CONTROLES FUERA DE PANTALLA DE CARGA ---
        if (kDown & KEY_START) return false;
        actualizarReproduccionVoz();
        bool canceloGrabacion = false;
        if (estadoActual == PANTALLA_CHAT && notaVozActiva()) {
            bool puedeContinuar = continuarNotaVoz();
            if (kDown & KEY_B) {
                detenerNotaVoz(false);
                canceloGrabacion = true;
            } else if ((kUp & (KEY_TOUCH | KEY_Y)) ||
                       ((kUp & KEY_A) && focoChat == 1) || !puedeContinuar) {
                int duracion = detenerNotaVoz(true);
                if (duracion > 0) {
                    g_audioPendiente = true;
                    enviarNotaPendiente();
                } else g_audioError = true;
            }
        }

        if ((kDown & KEY_SELECT) && (estadoActual == PANTALLA_CONTACTOS ||
                                     estadoActual == PANTALLA_CHAT) && !notaVozActiva()) {
            pantallaRetornoConfig = estadoActual;
            focoConfiguracion = 0;
            estadoActual = PANTALLA_CONFIGURACION;
            kDown = 0;
            touch.px = 0; touch.py = 0;
        }

        // Selección visible; la lista solo se desplaza al cruzar el borde.
        if (estadoActual == PANTALLA_CONTACTOS) {
            if (listaContactos.empty()) contactoSeleccionado = scrollContactos = 0;
            else {
                contactoSeleccionado = std::max(0, std::min(contactoSeleccionado,
                    (int)listaContactos.size() - 1));
                if ((kNav & KEY_DDOWN) && contactoSeleccionado + 1 < (int)listaContactos.size())
                    ++contactoSeleccionado;
                if ((kNav & KEY_DUP) && contactoSeleccionado > 0)
                    --contactoSeleccionado;
                if (contactoSeleccionado < scrollContactos) scrollContactos = contactoSeleccionado;
                if (contactoSeleccionado >= scrollContactos + 3)
                    scrollContactos = contactoSeleccionado - 2;
            }
            if ((kDown & KEY_A) && !listaContactos.empty()) {
                touch.px = 80;
                touch.py = 35 + (contactoSeleccionado - scrollContactos) * 48 + 20;
                kDown |= KEY_TOUCH;
            } else if (kDown & KEY_X) {
                touch.px = 170; touch.py = 210;
                kDown |= KEY_TOUCH;
            } else if (kDown & KEY_L) {
                pantallaRetornoConfig = PANTALLA_CONTACTOS;
                g_pestanaConfig = 2;
                focoConfiguracion = 0;
                estadoActual = PANTALLA_CONFIGURACION;
                kDown = 0;
            } else if (kDown & KEY_Y) {
                abrirPerfilContacto(contactoSeleccionado);
                kDown = 0;
            }
            actualizarMiniaturasContactos();
        }

        if (estadoActual == PANTALLA_PERFIL) {
            if (g_perfilPropio && (kNav & KEY_DDOWN) && focoPerfil < 5) ++focoPerfil;
            if (g_perfilPropio && (kNav & KEY_DUP) && focoPerfil > 0) --focoPerfil;
            if (kDown & (KEY_A | KEY_B)) {
                cerrarFotoVista();
                g_perfilFotoCargada = false;
                estadoActual = PANTALLA_CONTACTOS;
                kDown = 0;
            } else if (g_perfilPropio && (kDown & KEY_X) && focoPerfil < 4) {
                std::string* campos[] = {&g_perfilNombre, &g_perfilApodo, &g_perfilBio, &g_perfilDescripcion};
                campos[focoPerfil]->clear();
                guardarAjustesSD();
                g_perfilError = !guardarPerfilServidor();
            } else if (g_perfilPropio && (kDown & KEY_A)) {
                touch.px = 145;
                touch.py = 12 + focoPerfil * 36 + 16;
                kDown |= KEY_TOUCH;
            }
        }

        if (estadoActual == PANTALLA_CHAT && !g_confirmarBorrarContacto && !notaVozActiva()) {
            int fila = focoChat < 12 ? focoChat / 3 : 4;
            int columna = focoChat < 12 ? focoChat % 3 : 1;
            if ((kNav & KEY_DLEFT) && fila < 4 && columna > 0) --focoChat;
            if ((kNav & KEY_DRIGHT) && fila < 4 && columna < 2) ++focoChat;
            if ((kNav & KEY_DUP) && fila > 0)
                focoChat = (fila - 1) * 3 + columna;
            if (kNav & KEY_DDOWN) {
                if (fila < 3) focoChat = (fila + 1) * 3 + columna;
                else if (fila == 3) focoChat = 12;
            }
            if (kDown & (KEY_A | KEY_X | KEY_Y | KEY_L | KEY_R)) {
                int accion = (kDown & KEY_A) ? focoChat : (kDown & KEY_X) ? 2 :
                             (kDown & KEY_Y) ? 1 : (kDown & KEY_L) ? 4 : 5;
                touch.px = CHAT_FOCO_X[accion] + CHAT_FOCO_ANCHO[accion] / 2;
                touch.py = CHAT_FOCO_Y[accion] + CHAT_FOCO_ALTO[accion] / 2;
                kDown |= KEY_TOUCH;
            }
        }
        if (estadoActual == PANTALLA_CHAT && g_confirmarBorrarContacto && (kDown & KEY_A)) {
            touch.px = 230; touch.py = 165;
            kDown |= KEY_TOUCH;
        }

        if (estadoActual == PANTALLA_CONFIGURACION) {
            int ultimoFoco = g_pestanaConfig == 0 ? 4 : 5;
            if (kDown & KEY_L) { g_pestanaConfig = (g_pestanaConfig + 2) % 3; focoConfiguracion = 0; }
            if (kDown & KEY_R) { g_pestanaConfig = (g_pestanaConfig + 1) % 3; focoConfiguracion = 0; }
            if (kDown & KEY_Y) { g_pestanaConfig = 2; focoConfiguracion = 0; }
            if ((kNav & KEY_DDOWN) && focoConfiguracion < ultimoFoco) ++focoConfiguracion;
            if ((kNav & KEY_DUP) && focoConfiguracion > 0) --focoConfiguracion;
            if ((kDown & KEY_X) && g_pestanaConfig == 2 && focoConfiguracion < 4) {
                std::string* campos[] = {&g_perfilNombre, &g_perfilApodo, &g_perfilBio, &g_perfilDescripcion};
                campos[focoConfiguracion]->clear();
                guardarAjustesSD();
                g_perfilError = !guardarPerfilServidor();
                kDown = 0;
            } else if (kDown & (KEY_B | KEY_SELECT | (g_pestanaConfig == 2 ? 0 : KEY_X))) {
                guardarAjustesSD();
                estadoActual = pantallaRetornoConfig;
                kDown = 0;
            } else if (kDown & KEY_A) {
                touch.px = (g_pestanaConfig == 1 && focoConfiguracion == 4) ? 230 : 80;
                touch.py = g_pestanaConfig == 2 ? (focoConfiguracion < 5 ? 59 + focoConfiguracion * 30 : 218) :
                    g_pestanaConfig == 0 ? (focoConfiguracion < 4 ? 60 + focoConfiguracion * 36 : 218) :
                    focoConfiguracion < 3 ? 60 + focoConfiguracion * 40 :
                    focoConfiguracion == 3 || focoConfiguracion == 4 ? 180 : 218;
                kDown |= KEY_TOUCH;
            }
        }

        // Comprobar mensajes nuevos periódicamente
        if (estadoActual == PANTALLA_CONTACTOS) {
            if (osGetTime() - ultimoCheckTiempo > (u64)g_intervaloConsultaMs) {
                verificarMensajesNuevos();
                ultimoCheckTiempo = osGetTime();
            }
            if (g_ultimaCargaPerfiles == 0 || osGetTime() - g_ultimaCargaPerfiles > 30000)
                actualizarPerfilesContactos();
        }

        if (estadoActual == PANTALLA_CHAT && !notaVozActiva() &&
            osGetTime() - ultimoCheckTiempo > (u64)g_intervaloConsultaMs) {
            actualizarChatDesdeServidor(miID.c_str(), contactoActivoID.c_str());
            ultimoCheckTiempo = osGetTime();
        }

        // --- NAVEGACIÓN CON BOTONES FÍSICOS ---
        if (estadoActual == PANTALLA_FOTO && (kDown & (KEY_A | KEY_B))) {
            cerrarFotoVista();
            estadoActual = PANTALLA_CHAT;
        }
        else if (estadoActual == PANTALLA_CAMARA) {
            if (kDown & KEY_B) cerrarCapturaFoto(false);
            else if (kDown & KEY_A) tomarFotoVista();
        }
        else if (estadoActual == PANTALLA_CONFIRMAR_FOTO) {
            if (kDown & KEY_B) cerrarCapturaFoto(true);
            else if (kDown & KEY_X) repetirFotoVista();
            else if (kDown & KEY_A) confirmarEnvioFoto();
        }
        else if (estadoActual == PANTALLA_CHAT && (kDown & KEY_B) && !canceloGrabacion) {
            if (g_confirmarBorrarContacto) {
                g_confirmarBorrarContacto = false;
            } else if (g_audioPendiente) {
                remove(g_rutaAudioPendiente.c_str());
                g_audioPendiente = false;
                g_audioError = false;
            } else if (g_fotoPendiente) {
                remove(g_rutaFotoPendiente.c_str());
                g_fotoPendiente = false;
                g_fotoError = false;
            } else {
                estadoActual = PANTALLA_CONTACTOS;
                touch.px = 0; touch.py = 0;
            }
        }

        // --- INTERACCIÓN TÁCTIL (USANDO SWITCH) ---
        if ((kDown & KEY_TOUCH) && touch.px > 0 && touch.py > 0) {

            switch (estadoActual) {

            // Lista de contactos
            case PANTALLA_CONTACTOS: {
                // Zona inferior de botones (Y >= 190)
                if (touch.py >= 190) {
                    // Botón Configuración
                    if (touch.px >= 10 && touch.px <= 70) {
                        pantallaRetornoConfig = PANTALLA_CONTACTOS;
                        focoConfiguracion = 0;
                        estadoActual = PANTALLA_CONFIGURACION;
                        touch.px = 0; touch.py = 0;
                    }
                    // Botón Añadir Contacto
                    else if (touch.px >= 80 && touch.px <= 225) {
                        std::string nuevaID = abrirTeclado(getText(TXT_PASO_1));
                        hidScanInput();
                        if (!nuevaID.empty() && nuevaID.length() <= 10) {
                            bool esNum = true;
                            for (char c : nuevaID) if (!isdigit(c)) esNum = false;
                            if (esNum) {
                                std::string nuevoNombre = abrirTeclado(getText(TXT_PASO_2));
                                guardarContactoSD(nuevaID, nuevoNombre);
                                for (size_t i = 0; i < listaContactos.size(); i++) {
                                    if (listaContactos[i].id == nuevaID) {
                                        contactoSeleccionado = (int)i;
                                        scrollContactos = std::max(0, contactoSeleccionado - 2);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else if (touch.px >= 235 && touch.px <= 310) {
                        abrirPerfilContacto(contactoSeleccionado);
                    }
                }
                // Scroll de la lista
                else if (touch.px >= 280 && touch.py >= 35 && touch.py < 185) {
                    if (touch.py < 110 && scrollContactos > 0) {
                        scrollContactos--;
                    } else if (touch.py >= 110 && scrollContactos + 3 < (int)listaContactos.size()) {
                        scrollContactos++;
                    }
                    contactoSeleccionado = std::max(scrollContactos,
                        std::min(contactoSeleccionado, scrollContactos + 2));
                }
                // Seleccionar Contacto
                else if (touch.py >= 35 && touch.py < 185 && !listaContactos.empty()) {
                    int index = scrollContactos + ((touch.py - 35) / 48);
                    if (index >= 0 && index < (int)listaContactos.size()) {
                        contactoSeleccionado = index;
                        std::string nuevoID = listaContactos[index].id;

                        if (contactoActivoID != nuevoID) {
                            contactoActivoID = nuevoID;
                            contactoActivoNombre = listaContactos[index].nombre;
                            cargarChatDesdeSD(contactoActivoID);
                        }
                        g_v2Determinado = false;
                        g_servidorV2 = false;
                        g_masAntiguos = true;
                        g_desplazamientoChat = 0;
                        g_audioSeleccionadoId = 0;
                        g_fotoSeleccionadaId = 0;
                        cargarHistorialV2();
                        seleccionarAudio(0);
                        seleccionarFoto(0);
                        g_rutaAudioPendiente = "sdmc:/3ds/Mensinger3DS/pendiente_" +
                            claveServidorCache() + "_" + contactoActivoID + ".wav";
                        FILE* pendiente = fopen(g_rutaAudioPendiente.c_str(), "rb");
                        g_audioPendiente = pendiente != NULL;
                        if (pendiente) fclose(pendiente);
                        g_rutaFotoPendiente = "sdmc:/3ds/Mensinger3DS/pendiente_foto_" +
                            claveServidorCache() + "_" + miID + "_" + contactoActivoID + ".bgr";
                        pendiente = fopen(g_rutaFotoPendiente.c_str(), "rb");
                        g_fotoPendiente = pendiente != NULL;
                        if (pendiente) fclose(pendiente);

                        listaContactos[index].tieneNovedad = false;
                        apagarLED();

                        estadoActual = PANTALLA_CHAT;
                        focoChat = 0;
                        touch.px = 0; touch.py = 0;
                        actualizarChatDesdeServidor(miID.c_str(), contactoActivoID.c_str());
                        ultimoCheckTiempo = osGetTime();
                    }
                }
            break;
        }

        // 3. PANTALLA CHAT
        case PANTALLA_CHAT: {
            if (!g_confirmarBorrarContacto) {
                for (int i = 0; i < 13; i++) {
                    if (touch.px >= CHAT_FOCO_X[i] && touch.px < CHAT_FOCO_X[i] + CHAT_FOCO_ANCHO[i] &&
                        touch.py >= CHAT_FOCO_Y[i] && touch.py < CHAT_FOCO_Y[i] + CHAT_FOCO_ALTO[i]) {
                        focoChat = i;
                        break;
                    }
                }
            }
            if (g_confirmarBorrarContacto) {
                if (touch.py >= 150 && touch.py <= 190 && touch.px >= 20 && touch.px <= 154) {
                    g_confirmarBorrarContacto = false;
                    touch.px = 0; touch.py = 0;
                }
                else if (touch.py >= 150 && touch.py <= 190 && touch.px >= 166 && touch.px <= 300) {
                    borrarContactoActual(contactoActivoID);
                    historialChat.clear();
                    g_historialV2.clear();
                    cerrarFotoVista();
                    g_perfilFotoCargada = false;
                    contactoActivoID.clear();
                    contactoActivoNombre.clear();
                    g_confirmarBorrarContacto = false;
                    if (scrollContactos > 0 && scrollContactos + 3 > (int)listaContactos.size()) {
                        scrollContactos = std::max(0, (int)listaContactos.size() - 3);
                    }
                    contactoSeleccionado = listaContactos.empty() ? 0 :
                        std::min(contactoSeleccionado, (int)listaContactos.size() - 1);
                    estadoActual = PANTALLA_CONTACTOS;
                    touch.px = 0; touch.py = 0;
                    ultimoCheckTiempo = osGetTime();
                }
            }
            else if (touch.px >= 187 && touch.px <= 244 && touch.py >= 24 && touch.py <= 76) {
                if (g_audioPendiente) enviarNotaPendiente();
                else if (g_servidorV2 && !notaVozActiva()) {
                    g_audioError = !iniciarNotaVoz(g_rutaAudioPendiente);
                } else g_audioError = true;
            }
            else if (touch.px >= 249 && touch.px <= 300 && touch.py >= 24 && touch.py <= 76) {
                if (g_servidorV2) revisarFotoPendiente();
                else g_fotoError = true;
            }
            else if (touch.px >= 20 && touch.px <= 181 && touch.py >= 24 && touch.py <= 76) { // ESCRIBIR
                std::string prompt = "Mensaje para " + contactoActivoNombre;
                std::string textoMsg = abrirTeclado(prompt.c_str());
                hidScanInput();
                if (!textoMsg.empty()) {
                    std::string msgFormateado = "[Yo]: " + textoMsg;
                    historialChat.push_back(msgFormateado);

                    bool enviado = enviarMensajeServidor(miID.c_str(), contactoActivoID.c_str(), textoMsg.c_str());

                    if (enviado) {
                        if (g_servidorV2) cargarPaginaV2(0);
                        else guardarHistorialEnSD(contactoActivoID);
                        g_estadoEnvio = ENVIO_EXITO;
                    } else {
                        if (!historialChat.empty()) {
                            historialChat.pop_back();
                        }
                        g_estadoEnvio = ENVIO_ERROR;
                    }

                    g_tiempoNotificacion = osGetTime();
                    ultimoCheckTiempo = osGetTime();
                }
            }
            else if (touch.px >= 20 && touch.px <= 100 && touch.py >= 92 && touch.py <= 132) { // VOLVER
                estadoActual = PANTALLA_CONTACTOS;
                touch.px = 0; touch.py = 0;
                ultimoCheckTiempo = osGetTime();
            }
            else if (touch.py >= 92 && touch.py <= 132 && touch.px >= 105 && touch.px <= 202) {
                cargarMensajesAnteriores();
            }
            else if (touch.py >= 92 && touch.py <= 132 && touch.px >= 208 && touch.px <= 300) {
                g_desplazamientoChat = std::max(0, g_desplazamientoChat - 3);
            }
            else if (touch.py >= 144 && touch.py <= 182 && touch.px >= 20 && touch.px <= 100) {
                seleccionarAudio(-1);
            }
            else if (touch.py >= 144 && touch.py <= 182 && touch.px >= 105 && touch.px <= 215) {
                g_audioError = !reproducirAudioSeleccionado();
            }
            else if (touch.py >= 144 && touch.py <= 182 && touch.px >= 220 && touch.px <= 300) {
                seleccionarAudio(1);
            }
            else if (touch.py >= 184 && touch.py <= 217 && touch.px >= 20 && touch.px <= 100) {
                seleccionarFoto(-1);
            }
            else if (touch.py >= 184 && touch.py <= 217 && touch.px >= 106 && touch.px <= 214) {
                if (!mostrarFotoSeleccionada()) g_fotoError = true;
            }
            else if (touch.py >= 184 && touch.py <= 217 && touch.px >= 220 && touch.px <= 300) {
                seleccionarFoto(1);
            }
            else if (touch.px >= 264 && touch.px <= 304 && touch.py >= 220 && touch.py <= 238) {
                g_confirmarBorrarContacto = true;
                touch.px = 0; touch.py = 0;
            }
            break;
        }

        case PANTALLA_CAMARA:
            if (touch.py >= 90 && touch.py <= 169 && touch.px >= 35 && touch.px <= 285)
                tomarFotoVista();
            else if (touch.py >= 175 && touch.py <= 235 && touch.px >= 55 && touch.px <= 265)
                cerrarCapturaFoto(false);
            touch.px = 0; touch.py = 0;
            break;

        case PANTALLA_CONFIRMAR_FOTO:
            if (touch.py >= 90 && touch.py <= 175 && touch.px >= 15 && touch.px <= 155)
                repetirFotoVista();
            else if (touch.py >= 90 && touch.py <= 175 && touch.px >= 165 && touch.px <= 305)
                confirmarEnvioFoto();
            else if (touch.py >= 180 && touch.py <= 235 && touch.px >= 55 && touch.px <= 265)
                cerrarCapturaFoto(true);
            touch.px = 0; touch.py = 0;
            break;

        case PANTALLA_FOTO:
            if (touch.px > 0 || touch.py > 0) {
                cerrarFotoVista();
                estadoActual = PANTALLA_CHAT;
            }
            touch.px = 0; touch.py = 0;
            break;

        case PANTALLA_PERFIL: {
            if (!g_perfilPropio && (touch.px > 0 || touch.py > 0)) {
                cerrarFotoVista();
                g_perfilFotoCargada = false;
                estadoActual = PANTALLA_CONTACTOS;
            } else if (g_perfilPropio && touch.py >= 12 && touch.py < 228) {
                int fila = (touch.py - 12) / 36;
                focoPerfil = std::max(0, std::min(5, fila));
                if (fila < 4) {
                    const char* titulos[] = {"Nombre de usuario", "Apodo", "Biografia", "Descripcion"};
                    std::string* campos[] = {&g_perfilNombre, &g_perfilApodo, &g_perfilBio, &g_perfilDescripcion};
                    std::string nuevo = abrirTeclado(titulos[fila]);
                    hidScanInput();
                    if (!nuevo.empty()) {
                        size_t limite = fila < 2 ? 32 : 120;
                        if (nuevo.size() > limite) nuevo.resize(limite);
                        for (char& c : nuevo) if ((unsigned char)c < 32) c = ' ';
                        *campos[fila] = nuevo;
                        guardarAjustesSD();
                        g_perfilError = !guardarPerfilServidor();
                    }
                } else if (fila == 4) {
                    cerrarFotoVista();
                    g_fotoPerfilEnCurso = true;
                    g_pantallaRetornoFotoPerfil = PANTALLA_PERFIL;
                    abrirCamaraFoto();
                } else {
                    guardarAjustesSD();
                    g_perfilError = !sincronizarPerfilServidor();
                }
            }
            touch.px = 0; touch.py = 0;
            break;
        }

        // 4. PANTALLA CONFIGURACIÓN
        case PANTALLA_CONFIGURACION: {
            if (touch.py >= 8 && touch.py <= 36) {
                g_pestanaConfig = std::min(2, (int)touch.px / 103);
                g_errorConfig = false;
                focoConfiguracion = 0;
            }
            else if (g_pestanaConfig == 2 && touch.py >= 44 && touch.py < 194) {
                int fila = (touch.py - 44) / 30;
                focoConfiguracion = fila;
                if (fila < 4) {
                    const char* titulos[] = {"Nombre de usuario", "Apodo", "Biografia", "Descripcion"};
                    std::string* campos[] = {&g_perfilNombre, &g_perfilApodo, &g_perfilBio, &g_perfilDescripcion};
                    std::string nuevo = abrirTeclado(titulos[fila]);
                    hidScanInput();
                    if (!nuevo.empty()) {
                        size_t limite = fila < 2 ? 32 : 120;
                        if (nuevo.size() > limite) nuevo.resize(limite);
                        for (char& c : nuevo) if ((unsigned char)c < 32) c = ' ';
                        *campos[fila] = nuevo;
                        guardarAjustesSD();
                        g_perfilError = !guardarPerfilServidor();
                    }
                } else {
                    cerrarFotoVista();
                    g_fotoPerfilEnCurso = true;
                    g_pantallaRetornoFotoPerfil = PANTALLA_CONFIGURACION;
                    abrirCamaraFoto();
                }
            }
            else if (g_pestanaConfig == 0 && touch.py >= 44 && touch.py <= 181) {
                int fila = (touch.py - 44) / 36;
                focoConfiguracion = fila;
                if (fila == 0) g_modoOscuro = !g_modoOscuro;
                else if (fila == 1) g_ledActivado = !g_ledActivado;
                else if (fila == 2) g_sonidoActivado = !g_sonidoActivado;
                else if (fila == 3) g_buscarActualizaciones = !g_buscarActualizaciones;
            }
            else if (g_pestanaConfig == 1 && touch.py >= 44 && touch.py <= 79) {
                focoConfiguracion = 0;
                std::string nuevoHost = abrirTeclado(getText(TXT_SERVIDOR));
                if (!nuevoHost.empty()) {
                    if (servidorValido(nuevoHost)) { g_servidorHost = nuevoHost; g_errorConfig = false; g_estadoPruebaConexion = 0; }
                    else g_errorConfig = true;
                }
            }
            else if (g_pestanaConfig == 1 && touch.py >= 84 && touch.py <= 119) {
                focoConfiguracion = 1;
                std::string nuevoPuerto = abrirTeclado(getText(TXT_PUERTO));
                if (!nuevoPuerto.empty()) {
                    char* fin = NULL;
                    long puerto = strtol(nuevoPuerto.c_str(), &fin, 10);
                    if (fin && *fin == '\0' && puerto >= 1 && puerto <= 65535) {
                        g_servidorPuerto = (int)puerto; g_errorConfig = false; g_estadoPruebaConexion = 0;
                    } else g_errorConfig = true;
                }
            }
            else if (g_pestanaConfig == 1 && touch.py >= 124 && touch.py <= 159) {
                focoConfiguracion = 2;
                g_intervaloConsultaMs = g_intervaloConsultaMs == 1000 ? 2000 :
                    g_intervaloConsultaMs == 2000 ? 5000 : g_intervaloConsultaMs == 5000 ? 10000 : 1000;
            }
            else if (g_pestanaConfig == 1 && touch.py >= 169 && touch.py <= 196) {
                focoConfiguracion = touch.px < 160 ? 3 : 4;
                if (touch.px < 160) {
                    int sock = conectarAlServidor();
                    g_estadoPruebaConexion = sock >= 0 ? 1 : -1;
                    if (sock >= 0) close(sock);
                } else {
                    g_servidorHost = SERVIDOR_PREDETERMINADO;
                    g_servidorPuerto = PUERTO_PREDETERMINADO;
                    g_intervaloConsultaMs = 2000;
                    g_estadoPruebaConexion = 0;
                    g_errorConfig = false;
                }
            }
            else if (touch.py >= 204 && touch.py <= 235) {
                guardarAjustesSD();
                if (g_pestanaConfig == 2) {
                    focoConfiguracion = 5;
                    g_perfilError = !sincronizarPerfilServidor();
                } else estadoActual = pantallaRetornoConfig;
            }
            touch.px = 0; touch.py = 0;
            break;
        }
    }
}
    return true;
}
