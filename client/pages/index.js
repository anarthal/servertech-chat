import Head from 'next/head';
import styles from '../styles/Home.module.css';
import Header from '../components/header';

export default function Home() {
  return (
    <>
      <Head>
        <title>BoostServerTech chat</title>
        <link rel="icon" href="/favicon.ico" />
      </Head>

      <div className={styles.container}>
        <Header />
        <div className={styles.bodycontainer}>
          <div className='text-center p-5'>
            <p className='text-3xl p-3'>Welcome to</p>
            <p className='text-7xl p-3'>BoostServerTech Chat</p>
            <p className='text-xl p-3'>A chat app written using the Boost C++ libraries</p>
          </div>
          <div className='flex justify-center p-5'>
            <div className='bg-white rounded-2xl p-7'>
              <p className='text-center text-5xl'>Ready to try it?</p>
              <input type='text' placeholder='Choose a username...' />
              <button>Get started</button>
            </div>
          </div>
        </div>
      </div>
    </>
  )
}
