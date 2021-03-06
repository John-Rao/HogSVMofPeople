#include <QCoreApplication>
#include <opencv2/opencv.hpp>
#include <opencv2/ml/ml.hpp>
#include <iostream>

using namespace std;

#define PosSamNO 3542    //正样本个数
#define NegSamNO 11840    //负样本个数
#define HardExampleNO 0   //难例个数


void train_svm_hog(string Path_P,string Path_N)
{
    //HOG检测器，用来计算HOG描述子的
    //检测窗口(48,48),块尺寸(16,16),块步长(8,8),cell尺寸(8,8),直方图bin个数9
    cv::HOGDescriptor hog;
    int DescriptorDim;//HOG描述子的维数，由图片大小、检测窗口大小、块大小、细胞单元中直方图bin个数决定

    //设置SVM参数
    cv::Ptr<cv::ml::SVM> svm = cv::ml::SVM::create();
    svm->setType(cv::ml::SVM::Types::C_SVC);
    svm->setKernel(cv::ml::SVM::KernelTypes::LINEAR);
    svm->setTermCriteria(cv::TermCriteria(cv::TermCriteria::MAX_ITER, 100, 1e-6));

    string ImgName;

//    //正样本图片的文件列表
//    std::ifstream finPos("positive_samples.txt");
//    //负样本图片的文件列表
//    std::ifstream finNeg("negative_samples.txt");

    //所有训练样本的特征向量组成的矩阵，行数等于所有样本的个数，列数等于HOG描述子维数
    cv::Mat sampleFeatureMat;
    //训练样本的类别向量，行数等于所有样本的个数，列数等于1；1表示有目标，-1表示无目标
    cv::Mat sampleLabelMat;


    //依次读取正样本图片，生成HOG描述子
    for (int num = 0; num < PosSamNO; num++)
    {
        ImgName=Path_P+to_string(num+1)+".png";
        cout << "Processing:" << ImgName << std::endl;
        cv::Mat image = cv::imread(ImgName);

        //HOG描述子向量
        vector<float> descriptors;
        //计算HOG描述子，检测窗口移动步长(8,8)
        hog.compute(image, descriptors, cv::Size(8, 8));

        //处理第一个样本时初始化特征向量矩阵和类别矩阵，因为只有知道了特征向量的维数才能初始化特征向量矩阵
        if (0 == num)
        {
            //HOG描述子的维数
            DescriptorDim = descriptors.size();
            //初始化所有训练样本的特征向量组成的矩阵，行数等于所有样本的个数，列数等于HOG描述子维数sampleFeatureMat
            sampleFeatureMat = cv::Mat::zeros(PosSamNO + NegSamNO + HardExampleNO, DescriptorDim, CV_32FC1);
            //初始化训练样本的类别向量，行数等于所有样本的个数，列数等于1
            sampleLabelMat = cv::Mat::zeros(PosSamNO + NegSamNO + HardExampleNO, 1, CV_32SC1);
        }

        //将计算好的HOG描述子复制到样本特征矩阵sampleFeatureMat
        for (int i = 0; i < DescriptorDim; i++)
        {
            //第num个样本的特征向量中的第i个元素
            sampleFeatureMat.at<float>(num, i) = descriptors[i];
        }
        //正样本类别为1，有目标
        sampleLabelMat.at<float>(num, 0) = 1;
    }
    cout<<"Pos processing over......"<<endl;
    //依次读取负样本图片，生成HOG描述子
    for (int num = 0; num < NegSamNO; num++)
    {
        ImgName=Path_N+to_string(num+1)+".png";
        cout << "Processing:" << ImgName << std::endl;
        cv::Mat src = cv::imread(ImgName);
//        cv::resize(src, src, cv::Size(48, 48));
        //HOG描述子向量
        std::vector<float> descriptors;
        //计算HOG描述子，检测窗口移动步长(8,8)
        hog.compute(src, descriptors, cv::Size(8, 8));
        std::cout << "descriptor dimention:" << descriptors.size() << std::endl;
        //将计算好的HOG描述子复制到样本特征矩阵sampleFeatureMat
        for (int i = 0; i < DescriptorDim; i++)
        {
            //第PosSamNO+num个样本的特征向量中的第i个元素
            sampleFeatureMat.at<float>(num + PosSamNO, i) = descriptors[i];
        }

        //负样本类别为-1，无目标
        sampleLabelMat.at<float>(num + PosSamNO, 0) = -1;
    }

    //处理HardExample负样本
    if (HardExampleNO > 0)
    {
        //HardExample负样本的文件列表
        std::ifstream finHardExample("hard_samples_d.txt");
        //依次读取HardExample负样本图片，生成HOG描述子
        for (int num = 0; num < HardExampleNO && getline(finHardExample, ImgName); num++)
        {
            std::cout << "Processing:" << ImgName << std::endl;

            cv::Mat src = cv::imread(ImgName, cv::IMREAD_GRAYSCALE);
            cv::resize(src, src, cv::Size(48, 48));
            //HOG描述子向量
            std::vector<float> descriptors;
            //计算HOG描述子，检测窗口移动步长(8,8)
            hog.compute(src, descriptors, cv::Size(8, 8));

            //将计算好的HOG描述子复制到样本特征矩阵sampleFeatureMat
            for (int i = 0; i < DescriptorDim; i++)
            {
                //第PosSamNO+num个样本的特征向量中的第i个元素
                sampleFeatureMat.at<float>(num + PosSamNO + NegSamNO, i) = descriptors[i];
            }
            //负样本类别为-1，无目标
            sampleLabelMat.at<float>(num + PosSamNO + NegSamNO, 0) = -1;
        }
    }

    //训练SVM分类器
    //迭代终止条件，当迭代满1000次或误差小于FLT_EPSILON时停止迭代
    std::cout << "Training SVM..." << std::endl;
    cv::Ptr<cv::ml::TrainData> td = cv::ml::TrainData::create(sampleFeatureMat, cv::ml::SampleTypes::ROW_SAMPLE, sampleLabelMat);
    //训练分类器
    svm->train(td);
    std::cout << "Train success..." << std::endl;
    //将训练好的SVM模型保存为xml文件
    svm->save("SVM_HOG.xml");

    return;
}

void svm_hog_detect(cv::Mat image)
{
    //HOG检测器，用来计算HOG描述子的
    cv::HOGDescriptor hog;

    //HOG描述子的维数，由图片大小、检测窗口大小、块大小、细胞单元中直方图bin个数决定
    int DescriptorDim;

    //从XML文件读取训练好的SVM模型
    cv::Ptr<cv::ml::SVM> svm = cv::ml::SVM::load("SVM_HOG.xml");

    if (svm->empty())
    {
        std::cout << "load svm detector failed!!!" << std::endl;
        return;
    }

    //特征向量的维数，即HOG描述子的维数
    DescriptorDim = svm->getVarCount();

    //获取svecsmat，元素类型为float
    cv::Mat svecsmat = svm->getSupportVectors();
    //特征向量维数
    int svdim = svm->getVarCount();
    int numofsv = svecsmat.rows;

    //alphamat和svindex必须初始化，否则getDecisionFunction()函数会报错
    cv::Mat alphamat = cv::Mat::zeros(numofsv, svdim, CV_32F);
    cv::Mat svindex = cv::Mat::zeros(1, numofsv, CV_64F);

    cv::Mat Result;
    double rho = svm->getDecisionFunction(0, alphamat, svindex);
    //将alphamat元素的数据类型重新转成CV_32F
    alphamat.convertTo(alphamat, CV_32F);
    Result = -1 * alphamat * svecsmat;

    std::vector<float> vec;
    for (int i = 0; i < svdim; ++i)
    {
        vec.push_back(Result.at<float>(0, i));
    }
    vec.push_back(rho);

    //saving HOGDetectorForOpenCV.txt
    std::ofstream fout("HOGDetectorForOpenCV.txt");
    for (int i = 0; i < vec.size(); ++i)
    {
        fout << vec[i] << std::endl;
    }
    hog.setSVMDetector(vec);
    cout<<"WWWWWWWWWWWW"<<endl;

    /**************读入视频进行HOG检测******************/
    cv::VideoCapture cap(0);
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
    if(!cap.isOpened()){
        cout<<"can not open the cap..."<<endl;
        return;
    }
    double dist=0.0;
    while(true){
        cout<<"current dist is: "<<dist<<endl;
        cap>>image;
        std::vector<cv::Rect> found, found_1, found_filtered;
        //对图片进行多尺度检测
        hog.detectMultiScale(image, found, dist, cv::Size(8, 8), cv::Size(16, 16), 1.05, 2);

        for (int i = 0; i<found.size(); i++)
        {
            if (found[i].x > 0 && found[i].y > 0 && (found[i].x + found[i].width)< image.cols
                && (found[i].y + found[i].height)< image.rows)
                found_1.push_back(found[i]);
        }

        //找出所有没有嵌套的矩形框r,并放入found_filtered中,如果有嵌套的话,
        //则取外面最大的那个矩形框放入found_filtered中
        for (int i = 0; i < found_1.size(); i++)
        {
            cv::Rect r = found_1[i];
            int j = 0;
            for (; j < found_1.size(); j++)
                if (j != i && (r & found_1[j]) == found_1[j])
                    break;
            if (j == found_1.size())
                found_filtered.push_back(r);
        }

        //画矩形框，因为hog检测出的矩形框比实际目标框要稍微大些,所以这里需要做一些调整，可根据实际情况调整
        for (int i = 0; i<found_filtered.size(); i++)
        {
            cv::Rect r = found_filtered[i];
            //将检测矩形框缩小后绘制，根据实际情况调整
            r.x += cvRound(r.width*0.1);
            r.width = cvRound(r.width*0.8);
            r.y += cvRound(r.height*0.1);
            r.height = cvRound(r.height*0.8);
        }

        for (int i = 0; i<found_filtered.size(); i++)
        {
            cv::Rect r = found_filtered[i];

            cv::rectangle(image, r.tl(), r.br(), cv::Scalar(0, 0, 255), 2);

        }
        cv::imshow("detect result", image);
        int key=cv::waitKey(30);
        if(key==113){
            dist-=0.01;
        }else if(key==101){
            dist+=0.01;
        }else if(key==97){
            dist-=0.1;
        }else if(key==100){
            dist+=0.1;
        }
    }
/*
    cv::VideoCapture capture("video.avi");

    if (!capture.isOpened())
    {
        std::cout << "Read video Failed !" << std::endl;
        return;
    }

    cv::Mat frame;

    int frame_num = capture.get(cv::CAP_PROP_FRAME_COUNT);
    std::cout << "total frame number is: " << frame_num << std::endl;


    int width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    cv::VideoWriter out;

    //用于保存检测结果
    out.open("test result.mp4", CV_FOURCC('D', 'I', 'V', 'X'), 25.0, cv::Size(width / 2, height / 2), true);

    for (int i = 0; i < frame_num; ++i)
    {
        capture >> frame;

        cv::resize(frame, frame, cv::Size(width / 2, height / 2));

        //目标矩形框数组
        std::vector<cv::Rect> found, found_1, found_filtered;
        //对图片进行多尺度检测
        hog.detectMultiScale(frame, found, 0, cv::Size(8, 8), cv::Size(16, 16), 1.2, 2);

        for (int i = 0; i<found.size(); i++)
        {
            if (found[i].x > 0 && found[i].y > 0 && (found[i].x + found[i].width)< frame.cols
                && (found[i].y + found[i].height)< frame.rows)
                found_1.push_back(found[i]);
        }

        //找出所有没有嵌套的矩形框r,并放入found_filtered中,如果有嵌套的话,
        //则取外面最大的那个矩形框放入found_filtered中
        for (int i = 0; i < found_1.size(); i++)
        {
            cv::Rect r = found_1[i];
            int j = 0;
            for (; j < found_1.size(); j++)
                if (j != i && (r & found_1[j]) == found_1[j])
                    break;
            if (j == found_1.size())
                found_filtered.push_back(r);
        }

        //画矩形框，因为hog检测出的矩形框比实际目标框要稍微大些,所以这里需要做一些调整，可根据实际情况调整
        for (int i = 0; i<found_filtered.size(); i++)
        {
            cv::Rect r = found_filtered[i];
            //将检测矩形框缩小后绘制，根据实际情况调整
            r.x += cvRound(r.width*0.1);
            r.width = cvRound(r.width*0.8);
            r.y += cvRound(r.height*0.1);
            r.height = cvRound(r.height*0.8);
        }

        for (int i = 0; i<found_filtered.size(); i++)
        {
            cv::Rect r = found_filtered[i];

            cv::rectangle(frame, r.tl(), r.br(), cv::Scalar(0, 0, 255), 2);

        }
        cv::imshow("detect result", frame);

        //保存检测结果
        out << frame;

        if (cv::waitKey(30) == 'q')
        {
            break;
        }
    }
    capture.release();
    out.release();//*/
    return;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
//    train_svm_hog("E:/mystu/QTCode/pos/crop","E:/mystu/QTCode/neg/neg_J");
    cv::Mat image=cv::imread("test.jpg");
    svm_hog_detect(image);//*/
    return a.exec();
}
