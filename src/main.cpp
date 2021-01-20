/*
     * Copyright (C) 2020 Fondazione Istituto Italiano di Tecnologia
     *
     * Licensed under either the GNU Lesser General Public License v3.0 :
     * https://www.gnu.org/licenses/lgpl-3.0.html
     * or the GNU Lesser General Public License v2.1 :
     * https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
     * at your option.
     */

#include <fvad.h>
#include <yarp/sig/Sound.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/Network.h>
#include <yarp/os/LogStream.h>
#include <functional>


int main()
{

    yarp::os::Network yarp;
    if (!yarp.checkNetwork())
    {
        yError()<<"[main] Unable to find YARP network";
        return EXIT_FAILURE;
    }

    yarp::os::BufferedPort<yarp::sig::Sound> audioPort;

    audioPort.open("/face_expressions_from_mic/in");

    Fvad * fvadObject = fvad_new();

    /*
     * Changes the VAD operating ("aggressiveness") mode of a VAD instance.
     *
     * A more aggressive (higher mode) VAD is more restrictive in reporting speech.
     * Put in other words the probability of being speech when the VAD returns 1 is
     * increased with increasing mode. As a consequence also the missed detection
     * rate goes up.
     *
     * Valid modes are 0 ("quality"), 1 ("low bitrate"), 2 ("aggressive"), and 3
     * ("very aggressive"). The default mode is 0.
     *
     * Returns 0 on success, or -1 if the specified mode is invalid.
     */
    fvad_set_mode(fvadObject, 2);

    yarp::sig::Sound* inputSound = audioPort.read(false);
    std::vector<int16_t> copiedSound;
    if (inputSound)
    {
        /*
         * Sets the input sample rate in Hz for a VAD instance.
         *
         * Valid values are 8000, 16000, 32000 and 48000. The default is 8000. Note
         * that internally all processing will be done 8000 Hz; input data in higher
         * sample rates will just be downsampled first.
         *
         * Returns 0 on success, or -1 if the passed value is invalid.
         */
        fvad_set_sample_rate(fvadObject, inputSound->getFrequency());



        copiedSound.resize(inputSound->getSamples());

        for (size_t i = 0; i < copiedSound.size(); ++i)
        {
            copiedSound[i] = inputSound->get(i);
        }

        /*
         * Calculates a VAD decision for an audio frame.
         *
         * `frame` is an array of `length` signed 16-bit samples. Only frames with a
         * length of 10, 20 or 30 ms are supported, so for example at 8 kHz, `length`
         * must be either 80, 160 or 240.
         *
         * Returns              : 1 - (active voice),
         *                        0 - (non-active Voice),
         *                       -1 - (invalid frame length).
         */
        int isTalking = fvad_process(fvadObject, copiedSound.data(), inputSound->getInterleavedAudioRawData().size());
    }


    fvad_free(fvadObject);

}
