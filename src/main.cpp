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
#include <yarp/os/all.h>
#include <functional>
#include <cmath>


class module : public yarp::os::RFModule
{

    Fvad * fvadObject {nullptr};
    yarp::os::BufferedPort<yarp::sig::Sound> audioPort;
    std::vector<int16_t> copiedSound;
    int isTalking = 0;
    size_t switchCounter = 0;
    bool mouthOpen = false;
    size_t switchValue = 2;
    yarp::os::RpcClient emotions;

public:

    virtual bool configure(yarp::os::ResourceFinder& rf) override
    {
        yInfo() << "Configuring";
        fvadObject = fvad_new();

        if (!fvadObject)
        {
            yError() << "Failed to created VAD object";
            return false;
        }
        audioPort.open("/face_expressions_from_mic/in");

        emotions.open("/face_expressions_from_mic/emotions:o");

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
        fvad_set_mode(fvadObject, 3);

        /*
         * Sets the input sample rate in Hz for a VAD instance.
         *
         * Valid values are 8000, 16000, 32000 and 48000. The default is 8000. Note
         * that internally all processing will be done 8000 Hz; input data in higher
         * sample rates will just be downsampled first.
         *
         * Returns 0 on success, or -1 if the passed value is invalid.
         */
        if (fvad_set_sample_rate(fvadObject, 8000))
        {
            yError() << "Unsupported input frequency.";
            return EXIT_FAILURE;
        }

        yInfo() << "Started";


        return true;
    }

    virtual double getPeriod() override
    {
        return 0.01;
    }

    virtual bool updateModule() override
    {
        if (audioPort.getPendingReads())
        {
            yarp::sig::Sound* inputSound = audioPort.read(false);

            if (inputSound->getFrequency() < 8000)
            {
                yError() << "The frequency needs to be at least 8000";
                return false;
            }

            int subsampling = std::round(inputSound->getFrequency() / 8000.0);

            size_t desiredLength = 80;

            size_t soundLength = desiredLength * subsampling;

            if (soundLength > inputSound->getSamples())
            {
                yError() << "The input sound is too short.";
                return EXIT_FAILURE;
            }

            copiedSound.resize(desiredLength);

            for (size_t i = 0; i < desiredLength; ++i)
            {
                copiedSound[i] = inputSound->get(i*subsampling);
            }

//            /*
//         * Calculates a VAD decision for an audio frame.
//         *
//         * `frame` is an array of `length` signed 16-bit samples. Only frames with a
//         * length of 10, 20 or 30 ms are supported, so for example at 8 kHz, `length`
//         * must be either 80, 160 or 240.
//         *
//         * Returns              : 1 - (active voice),
//         *                        0 - (non-active Voice),
//         *                       -1 - (invalid frame length).
//         */
            isTalking = fvad_process(fvadObject, copiedSound.data(), copiedSound.size());

            if (isTalking < 0)
            {
                yError() << "Invalid frame length.";
                return false;
            }

            if (isTalking)
            {
                switchCounter++;

                if (switchCounter > switchValue)
                {
                    mouthOpen = !mouthOpen;
                    switchCounter = 0;
                }
            }
            else
            {
                mouthOpen = false;
                switchCounter = 0;
            }

            yInfo() << "Mouth open: " << mouthOpen;

            std::string state;

            if (mouthOpen)
                state="sur";
            else
                state="neu";

            yarp::os::Bottle cmd, reply;
            cmd.addVocab(yarp::os::Vocab::encode("set"));
            cmd.addVocab(yarp::os::Vocab::encode("mou"));
            cmd.addVocab(yarp::os::Vocab::encode(state));
            emotions.write(cmd,reply);

        }

        return true;
    }

    virtual bool close() override
    {
        fvad_free(fvadObject);
        audioPort.close();
        yInfo() << "Closing";
        emotions.close();

        return true;
    }
};

int main()
{

    yarp::os::Network yarp;
    if (!yarp.checkNetwork())
    {
        yError()<<"[main] Unable to find YARP network";
        return EXIT_FAILURE;
    }

    yarp::os::ResourceFinder empty;

    module speechmodule;

    return speechmodule.runModule(empty);
}
